#include "relay.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"

#include <glog/logging.h>
#include "utils/dlog.h"

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>


DEFINE_int32(mbr_upstream_connections, 15, "Minimum amount of connections for each upstream");
DEFINE_int32(mbr_requests_recycling, 100000, "Amount of request made by each connection before recycling it");


MTX::Relay::Relay(const rapidjson::Document& conf, struct event_base *base){
    LOG(INFO) << "building configuration ...";

    this->base = base;
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    conf.Accept(writer);
    LOG(INFO) << buffer.GetString();

    for(auto it = conf.Begin(); it != conf.End(); ++it){
        const rapidjson::Value& val = *it;
        int shard = val["shard"].GetInt();
        std::string ep = val["endpoint"].GetString();
        std::vector<std::string> parts;
        boost::split(parts, ep, boost::is_any_of(":"));
        LOG(INFO) << "Loading shard " << shard << " : " << ep;
        shards.insert(std::make_pair(shard,
            std::make_pair(parts[0], std::atoi(parts[1].c_str()))));
    }
}

MTX::Relay::~Relay(){
}

void
MTX::Relay::request_cb(struct evhttp_request *req, void *arg){
    ((MTX::Relay*)arg)->process_request(req);
}

void
MTX::Relay::relay_cb(struct evhttp_request *req, void *arg){
    relay_placeholder* p = (relay_placeholder*)arg;
    p->self->process_relay(req, p->original_req, p->connection, p->conn_pool);
    delete p;
}

void
MTX::Relay::multiple_relay_cb(struct evhttp_request *req, void *arg){
    multiple_relay_placeholder* p = (multiple_relay_placeholder*)arg;
    bool cleanup = p->self->process_multiple_relay(req, p);
    if(cleanup)
        delete p;
}

std::string
MTX::Relay::get_command(struct evhttp_request *req){
    std::string cmdtype;
    switch (evhttp_request_get_command(req)){
	    case EVHTTP_REQ_GET: cmdtype = "GET"; break;
	    case EVHTTP_REQ_POST: cmdtype = "POST"; break;
	    case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
	    case EVHTTP_REQ_PUT: cmdtype = "PUT"; break;
	    case EVHTTP_REQ_DELETE: cmdtype = "DELETE"; break;
	    case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
	    case EVHTTP_REQ_TRACE: cmdtype = "TRACE"; break;
	    case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
	    case EVHTTP_REQ_PATCH: cmdtype = "PATCH"; break;
	    default: cmdtype = "unknown"; break;
	}
    return cmdtype;
}

std::map<std::string, std::string>
MTX::Relay::get_qs(const std::string& qs){
    std::vector<std::string> params;
    std::map<std::string, std::string> avp;
    boost::split(params, qs, boost::is_any_of("&"));
    for(auto it = params.begin(); it != params.end(); ++it){
        std::vector<std::string> kv;
        boost::split(kv, *it, boost::is_any_of("="));
        if(kv.size() == 2){
            boost::replace_all(kv[1], "%3a", ":");
            avp.insert(std::make_pair(kv[0], kv[1]));
        }else if(kv.size() == 1){
            avp.insert(std::make_pair(kv[0], ""));
        }
    }
    return avp;
}

void
MTX::Relay::process_request(struct evhttp_request *req){

    std::string uri = evhttp_request_get_uri(req);
    struct evhttp_uri* http_uri = evhttp_uri_parse(uri.c_str());
    std::string path = evhttp_uri_get_path(http_uri);

    std::map<std::string, std::string> qs_map;
    std::string query;
    if(evhttp_uri_get_query(http_uri)){
        query = evhttp_uri_get_query(http_uri);
        qs_map = get_qs(query);
    }
    evhttp_uri_free(http_uri);

    std::string cmdtype = get_command(req);

    DLOGINFO("uri : " << uri);
    DLOGINFO("path : " << path);
    DLOGINFO("method : " << cmdtype);
    DLOGINFO("query : " << query);
#ifdef VERBOSELOG
    for(auto it = qs_map.begin(); it != qs_map.end(); ++it)
        DLOGINFO("\t" << it->first << " : " << it->second);
#endif

    std::string parent_account =
        get_parent_account(path, cmdtype, qs_map);

    if(parent_account.size() && parent_account != "*"){
        // single shoot
        single_shoot(req, parent_account, uri);
    }else if(parent_account.size()){
        // it's a multiple request
        multiple_shoot(req, uri);
    }else{
        evhttp_send_reply(req, 500, "Error", NULL);
    }

}

void
MTX::Relay::single_shoot(
        struct evhttp_request *req,
        const std::string& parent_account,
        const std::string& uri){

    MTX::HttpConnectionPool & banker_conn_pool = get_relay_conn_pool(parent_account);

    // get the body
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    std::string body = get_body(buf);
    DLOGINFO("redirecting : " << banker_conn_pool.get_host()
                        << ":" << banker_conn_pool.get_port());

    // Get a connection from the pool
    struct evhttp_connection* conn = banker_conn_pool.get_connection();

    if(conn == NULL){
        evhttp_send_reply(req, 500, "Error", NULL);
        return;
    }

    relay_placeholder* holder = new relay_placeholder;
    holder->self = this;
    holder->original_req = req;
    holder->connection = conn;
    holder->conn_pool = &banker_conn_pool;
    // create the relay request
    struct evhttp_request *relay_req =
        evhttp_request_new(relay_cb, holder);

    // set the headers
    DLOGINFO("setting headers : ");
    struct evkeyval *header;
    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    banker_conn_pool.set_connection_header(relay_req, conn);
    for (header = headers->tqh_first; header;
        header = header->next.tqe_next){
        evhttp_add_header(
            relay_req->output_headers, header->key, header->value);
        DLOGINFO("  " << header->key << ":" << header->value);
    }

    //set the body
    struct evbuffer * relay_buf =
        evhttp_request_get_output_buffer(relay_req);
    evbuffer_add_printf(relay_buf, "%s", body.c_str());

    // shoot
    evhttp_make_request(conn, relay_req,
        evhttp_request_get_command(req), uri.c_str());
}


void
MTX::Relay::multiple_shoot(
    struct evhttp_request *req, const std::string& uri){

    // get the body
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    std::string body = get_body(buf);

    multiple_relay_placeholder* holder = new multiple_relay_placeholder;
    holder->self = this;
    holder->original_req = req;
    holder->response_counter = 0;

    shard_map::iterator it;
    for(it = shards.begin(); it != shards.end(); ++it){

        std::pair<std::string, unsigned short> banker_uri = it->second;
        // create the connection
        struct evhttp_connection* conn =
            evhttp_connection_base_new(base, NULL,
                banker_uri.first.c_str(), banker_uri.second);
        holder->connections.push_back(conn);

        // create the relay request
        struct evhttp_request *relay_req =
            evhttp_request_new(multiple_relay_cb, holder);

        // set the headers
        struct evkeyval *header;
        struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
        for (header = headers->tqh_first; header;
            header = header->next.tqe_next){
            evhttp_add_header(
                relay_req->output_headers, header->key, header->value);
        }

        //set the body
        struct evbuffer * relay_buf =
            evhttp_request_get_output_buffer(relay_req);
        evbuffer_add_printf(relay_buf, "%s", body.c_str());

        // shoot
        DLOGINFO("shooting shard " << it->first << " at " <<
            it->second.first << ":" << it->second.second);
        evhttp_make_request(conn, relay_req,
            evhttp_request_get_command(req), uri.c_str());
    }
}

void
MTX::Relay::process_relay(
        evhttp_request *relay_req,
        evhttp_request *original_req,
        evhttp_connection *relay_conn,
		MTX::HttpConnectionPool * conn_pool){
    if(relay_req){
        //copy the relayed request body into the original body
        struct evbuffer* buf =
            evhttp_request_get_input_buffer(relay_req);
        std::string body = get_body(buf);
        struct evbuffer* req_buf =
            evhttp_request_get_output_buffer(original_req);
        evbuffer_add_printf(req_buf, "%s", body.c_str());
        // send the reply
        evhttp_send_reply(original_req,
            evhttp_request_get_response_code(relay_req),
            "OK",
            req_buf);
    }else{
        DLOGINFO("relay request is NULL");
        evhttp_send_reply(original_req,
            500,
            "Error",
            NULL);
    }

    // return the connection to the pool
    conn_pool->return_connection(relay_conn);
}

bool
MTX::Relay::process_multiple_relay(
                    evhttp_request *relay_req,
                    multiple_relay_placeholder* holder){

    //copy the relayed request body into the original body
    struct evbuffer* buf =
        evhttp_request_get_input_buffer(relay_req);
    std::string body = get_body(buf);
    holder->bodies.push_back(body);

    holder->response_counter += 1;

    if(holder->response_counter !=
            (int)(holder->connections.size())){
        return false;
    }

    // we got all the answers, we can reply now
    std::string result_body = add_replies(holder->bodies);
    DLOGINFO("result_body : " << result_body);
    struct evbuffer* req_buf =
        evhttp_request_get_output_buffer(holder->original_req);
    evbuffer_add_printf(req_buf, "%s", result_body.c_str());

    // send the reply
    evhttp_send_reply(holder->original_req,
        evhttp_request_get_response_code(relay_req),
        "OK",
        req_buf);

    // clean up connections
    for(std::size_t i = 0; i < holder->connections.size(); ++i){
        evhttp_connection_free(holder->connections[i]);
    }

    return true;
}

std::string
MTX::Relay::get_parent_account(
                   const std::string& path,
                   const std::string& cmdtype,
                   std::map<std::string, std::string>& qs_map){
    std::vector<std::string> path_parts;
    boost::split(path_parts, path, boost::is_any_of("/"));

    std::string action = path_parts[path_parts.size()-1];
    if(path_parts.size() == 4 &&
            path_parts[2] == "accounts"){
        action = "accounts";
    }
    DLOGINFO("action : " << action);

    // get the parent account
    std::string parent;
    if((path == "/v1/accounts"       ||
        path == "/v1/activeaccounts" ||
        path == "/v1/summary")
                && (cmdtype == "GET")){
        parent = "*";
        // collect the data and the return it
        DLOGINFO("parent * " << parent);
    }else if(action == "accounts" && cmdtype == "POST"){
        // POST /v1/accounts
        parent = get_parent_account(qs_map["accountName"]);
    }else if(action == "accounts" &&
                 cmdtype == "GET" &&
                 path_parts.size() == 4){
        // GET /v1/accounts/<accountName>
        parent = get_parent_account(path_parts[3]);
    }else if((
            action == "adjustment" ||
            action == "balance"    ||
            action == "shadow"     ||
            action == "budget")
                && (cmdtype == "POST" || cmdtype == "PUT")){
        // POST,PUT /v1/accounts/<accountName>/adjustment
        // POST,PUT /v1/accounts/<accountName>/balance
        // POST,PUT /v1/accounts/<accountName>/shadow
        // POST,PUT /v1/accounts/<accountName>/budget
        parent = get_parent_account(path_parts[3]);
    }else if((
            action == "children" ||
            action == "close"    ||
            action == "subtree"  ||
            action == "summary")
                && (cmdtype == "GET")){
        // GET /v1/accounts/<accountName>/children
        // GET /v1/accounts/<accountName>/close
        // GET /v1/accounts/<accountName>/subtree
        // GET /v1/accounts/<accountName>/summary
        parent = get_parent_account(path_parts[3]);
    }

    if(!parent.size())
        LOG(ERROR) << "unable to find account name";
    return parent;
}

unsigned int
MTX::Relay::get_shard(unsigned int hash)
{
    return hash % shards.size();
}

MTX::HttpConnectionPool &
MTX::Relay::get_connection_pool(const std::string & host, int port, unsigned int hash)
{
	unsigned int shard_slot = get_shard(hash);
    auto it = bankers_conn_by_shards.find(shard_slot);
    if ( it == bankers_conn_by_shards.end()){
    	MTX::HttpConnectionPool con_pool(this->base, host, port);
    	con_pool.set_requests_before_recycling(FLAGS_mbr_requests_recycling);
    	con_pool.set_upstream_connections(FLAGS_mbr_upstream_connections);
    	bankers_conn_by_shards.insert(std::make_pair(shard_slot, con_pool));
    	return bankers_conn_by_shards.at(shard_slot);
    }
    return it->second;
}

MTX::HttpConnectionPool &
MTX::Relay::get_relay_conn_pool(const std::string& parent){

    DLOGINFO("parent account : " << parent);
    unsigned int hash = SDBMHash(parent);
    DLOGINFO("hashed account : " << hash);

    std::pair<std::string, unsigned short> banker_ep =
        get_banker_uri(hash);
    DLOGINFO("shard uri : " << banker_ep.first << ":" << banker_ep.second);

    MTX::HttpConnectionPool & conn = get_connection_pool(banker_ep.first,
    		banker_ep.second, hash);

    return conn;
}

std::string
MTX::Relay::get_parent_account(const std::string& account_name){
    // get the parent account
    std::vector<std::string> account;
    boost::split(account, account_name, boost::is_any_of(":"));
    return account[0];
}

unsigned int
MTX::Relay::SDBMHash(const std::string& str){
	unsigned int hash = 0;
	unsigned int i = 0;
	unsigned int len = str.length();

	for (i = 0; i < len; i++){
		hash = (str[i]) + (hash << 6) + (hash << 16) - hash;
	}
	return hash & 0x7FFFFFFF;
}

std::pair<std::string, unsigned short>
MTX::Relay::get_banker_uri(unsigned int hash){
    unsigned int shard = hash % shards.size();
    return shards[shard];
}

std::string
MTX::Relay::get_body(struct evbuffer *buf){
    std::string body;
    while (evbuffer_get_length(buf)){
        char cbuf[1024];
        int n = evbuffer_remove(buf, cbuf, sizeof(cbuf) - 1);
        cbuf[n] = '\0';
        body += cbuf;
    }
    return body;
}

std::string
MTX::Relay::add_replies(const std::vector<std::string>& bodies){
    if(!bodies.size())
        return "";

    rapidjson::Document result;
    try {
        result.Parse(bodies[0].c_str());
    }catch(...){
        LOG(ERROR) << "unableto parse body : " << bodies[0];
        return "";
    }
    rapidjson::Document::AllocatorType& allocator = result.GetAllocator();
    if(result.IsArray()){
        for(std::size_t i = 1; i < bodies.size(); ++i){
            rapidjson::Document tmp;
            try{
                tmp.Parse(bodies[i].c_str());
            }catch(...){
                LOG(ERROR) << "unable to parse body : " << bodies[i];
                continue;
            }
            for(auto it = tmp.Begin(); it != tmp.End(); ++it){
                result.PushBack(*it, allocator);
            }
        }
    }else if(result.IsObject()){
        for(std::size_t i = 1; i < bodies.size(); ++i){
            rapidjson::Document tmp;
            try{
                tmp.Parse(bodies[i].c_str());
            }catch(...){
                LOG(ERROR) << "unable to parse body : " << bodies[i];
                continue;
            }
            for(auto it = tmp.MemberBegin(); it != tmp.MemberEnd(); ++it){
                result.AddMember(it->name , it->value , allocator);
            }
        }
    }else{
        return "";
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    result.Accept(writer);

    return buffer.GetString();
}


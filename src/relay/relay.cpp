#include "relay.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <glog/logging.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <boost/algorithm/string.hpp>

MTX::Relay::Relay(const rapidjson::Document& conf, struct event_base *base){
    LOG(INFO) << "building configuration ...";

    this->base = base;
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    conf.Accept(writer);
    LOG(INFO) << buffer.GetString();;

    for(auto it = conf.Begin(); it != conf.End(); ++it){
        const rapidjson::Value& val = *it;
        int shard = val["shard"].GetInt();
        std::string uri = val["uri"].GetString();
        LOG(INFO) << "Loading shard " << shard << " : " << uri;
        shards.insert(std::make_pair(shard, uri));
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
    //TODO
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
    std::string cmdtype = get_command(req);

    LOG(INFO) << "uri : " << uri;
    LOG(INFO) << "path : " << path;
    LOG(INFO) << "method : " << cmdtype;
    LOG(INFO) << "query : " << query;
    for(auto it = qs_map.begin(); it != qs_map.end(); ++it)
        LOG(INFO) << "\t" << it->first << " : " << it->second;

    std::string banker_uri = get_relay_uri(path, cmdtype, qs_map);

    if(!banker_uri.size()){
        evhttp_send_reply(req, 500, "Error", NULL);
        return;
    }
    //TODO move
    evhttp_send_reply(req, 200, "Ok", NULL);

    //TODO move this to the relay constructor
    std::vector<std::string> params;
    boost::split(params, banker_uri, boost::is_any_of(":"));
    short int port = std::atoi(params[1].c_str());
    std::string host = params[1];

    // get the body
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    std::string body = get_body(buf);

    // create the connection
    struct evhttp_connection* conn =
        evhttp_connection_base_new(base, NULL, "127.0.0.1", 8080);

    // create the relay request
    struct evhttp_request *relay_req =
        evhttp_request_new(relay_cb, this);

    // set the headers
    LOG(INFO) << "setting headers : ";
    struct evkeyval *header;
    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    for (header = headers->tqh_first; header;
        header = header->next.tqe_next){
        evhttp_add_header(
            relay_req->output_headers, header->key, header->value);
        LOG(INFO) << "  " << header->key << ":" << header->value;
    }

    //set the body
    struct evbuffer * relay_buf =
        evhttp_request_get_output_buffer(relay_req);
    evbuffer_add_printf(relay_buf, "%s", body.c_str());

    // shoot
    evhttp_make_request(conn, relay_req,
        evhttp_request_get_command(req), uri.c_str());
}


std::string
MTX::Relay::get_relay_uri(
            const std::string& path,
            const std::string& cmdtype,
            std::map<std::string, std::string>& qs_map){

    std::vector<std::string> path_parts;
    boost::split(path_parts, path, boost::is_any_of("/"));

    std::string action = path_parts[path_parts.size()-1];
    LOG(INFO) << "action : " << action;

    // get the parent account
    std::string parent;
    if(action == "accounts" && cmdtype == "POST"){
        parent = get_parent_account(qs_map["accountName"]);
    }else if(action == "balance" && cmdtype == "POST"){
        parent = get_parent_account(path_parts[3]);
    }else if(action == "shadow" && cmdtype == "PUT"){
        parent = get_parent_account(path_parts[3]);
    }

    if(!parent.size()){
        LOG(ERROR) << "unable to find account name";
        return "";
    }

    LOG(INFO) << "parent account : " << parent;
    unsigned int hash = SDBMHash(parent);
    LOG(INFO) << "hashed account : " << hash;

    std::string banker_uri = get_banker_uri(hash);
    LOG(INFO) << "shard uri : " << banker_uri;
    return banker_uri;
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
	return hash;
}

std::string
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

#include "banker.h"

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

MTX::MasterBanker::MasterBanker(struct event_base *base){
    LOG(INFO) << "building configuration ...";

    this->base = base;
}

MTX::MasterBanker::~MasterBanker(){
}

void MTX::MasterBanker::initialize(){
    //register the routes;
    Router::request_async_action adjustment = [](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body){
        DLOGINFO("adjustment : " << path);
    };
    router.addAsyncRoute("POST", "adjustment", adjustment);
    router.addAsyncRoute("PUT", "adjustment", adjustment);
}

void
MTX::MasterBanker::request_cb(struct evhttp_request *req, void *arg){
    ((MTX::MasterBanker*)arg)->process_request(req);
}

std::string
MTX::MasterBanker::get_command(struct evhttp_request *req){
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
MTX::MasterBanker::get_qs(const std::string& qs){
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

std::string 
MTX::MasterBanker::get_body(struct evbuffer *buf){
    std::string body;
    while (evbuffer_get_length(buf)){
        char cbuf[1024];
        int n = evbuffer_remove(buf, cbuf, sizeof(cbuf) - 1);
        cbuf[n] = '\0';
        body += cbuf;
    }
    return body;
}


void
MTX::MasterBanker::process_request(struct evhttp_request *req){

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

    // get the body
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    std::string body = get_body(buf);

    std::map<std::string, std::string> heads;
    // set the headers
    struct evkeyval *header;
    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    for (header = headers->tqh_first; header;
                    header = header->next.tqe_next){
        heads.insert(std::make_pair(header->key, header->value));
    }

#ifdef VERBOSELOG
    for(auto it = qs_map.begin(); it != qs_map.end(); ++it)
        DLOGINFO("\t" << it->first << " : " << it->second);
#endif

    bool ok = router.route(cmdtype, path, qs_map, heads, body);
    if(ok)
        evhttp_send_reply(req, 204, "Empty", NULL);
    else
        evhttp_send_reply(req, 404, "Not Found", NULL);
}


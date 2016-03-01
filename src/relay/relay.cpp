#include "relay.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <glog/logging.h>


MTX::Relay::Relay(const rapidjson::Document& conf){
    LOG(INFO) << "building configuration ...";

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
MTX::Relay::process_request(struct evhttp_request *req){
    evhttp_send_reply(req, 200, "OK", NULL);
}

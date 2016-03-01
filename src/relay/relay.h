#ifndef __MBR_RELAY_H__
#define __MBR_RELAY_H__
#include <boost/function.hpp>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <rapidjson/document.h>
#include <string>
#include <map>

namespace MTX {

struct Relay{

    // constructor
    Relay(const rapidjson::Document& conf);

    // destructor
    ~Relay();

    // callback for the http event loop
    static void
    request_cb(struct evhttp_request *req, void *arg);

private :

    void process_request(struct evhttp_request *req);

    typedef std::map<int, std::string> shard_map;
    shard_map shards;

};

}
#endif

#ifndef __MASTER_BANKER_H__
#define __MASTER_BANKER_H__
#include <boost/function.hpp>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <rapidjson/document.h>
#include <string>
#include <map>

#include "utils/router.h"

namespace MTX {

struct MasterBanker{

    // constructor
    MasterBanker(struct event_base *base);

    // destructor
    ~MasterBanker();

    //initialize
    void initialize();

    // callback for the http event loop
    static void
    request_cb(struct evhttp_request *req, void *arg);

private :

    struct context{
        evhttp_request* req;
    };

    void process_request(struct evhttp_request *req);

    std::string
    get_command(struct evhttp_request *req);

    std::map<std::string, std::string>
    get_qs(const std::string& qs);

    std::string
    get_body(struct evbuffer *buf);

    struct event_base* base;

    Router router;
};

}
#endif

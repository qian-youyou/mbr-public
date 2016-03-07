#ifndef __MBR_RELAY_H__
#define __MBR_RELAY_H__
#include <boost/function.hpp>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <rapidjson/document.h>
#include <string>
#include <map>

namespace MTX {

struct Relay{

    // constructor
    Relay(const rapidjson::Document& conf, struct event_base *base);

    // destructor
    ~Relay();

    // callback for the http event loop
    static void
    request_cb(struct evhttp_request *req, void *arg);

private :

    struct relay_placeholder{
        Relay* self;
        evhttp_request* original_req;
        evhttp_connection* connection;
    };

    struct multiple_relay_placeholder{
        Relay* self;
        evhttp_request* original_req;
        std::vector<evhttp_connection*> connections;
        std::vector<std::string> bodies;
        int response_counter;
    };

    void process_request(struct evhttp_request *req);

    void process_relay(evhttp_request *relay_req,
                       evhttp_request *original_req,
                       evhttp_connection *relay_conn);

    bool process_multiple_relay(evhttp_request *relay_req,
                                multiple_relay_placeholder* holder);

    std::string
    get_command(struct evhttp_request *req);

    std::map<std::string, std::string>
    get_qs(const std::string& qs);

    std::string
    get_parent_account(const std::string& account_name);

    unsigned int
    SDBMHash(const std::string& str);

    std::pair<std::string, unsigned short>
    get_banker_uri(unsigned int hash);

    std::string
    get_parent_account(const std::string& path,
                       const std::string& cmdtype,
                       std::map<std::string, std::string>& qs_map);

    std::pair<std::string, unsigned short>
    get_relay_uri(const std::string& parent);

    // callback for the http relay response
    static void
    relay_cb(struct evhttp_request *req, void *arg);

    // callback for the http relay response
    static void
    multiple_relay_cb(struct evhttp_request *req, void *arg);

    void
    single_shoot(
        struct evhttp_request *req,
        const std::string& parent_account,
        const std::string& uri);

    void
    multiple_shoot(
        struct evhttp_request *req,
        const std::string& uri);

    std::string get_body(struct evbuffer *buf);

    std::string add_replies(const std::vector<std::string>& bodies);

    typedef std::map<int, std::pair<std::string, unsigned short>> shard_map;
    shard_map shards;

    struct event_base* base;
};

}
#endif

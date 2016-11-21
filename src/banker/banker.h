#ifndef __MASTER_BANKER_H__
#define __MASTER_BANKER_H__
#include <boost/function.hpp>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <rapidjson/document.h>
#include <string>
#include <map>
#include <carboncxx/carbon_logger.h>

#include "utils/router.h"
#include "account.h"
#include "account_key.h"
#include "soa/service/redis.h"

namespace MTX {

struct MasterBanker{

    // constructor
    MasterBanker(struct event_base *base,
                 std::shared_ptr<Redis::AsyncConnection> redis,
                 std::shared_ptr<CarbonLogger> logger);

    // destructor
    ~MasterBanker();

    //initialize
    void initialize();

    // callback for the http event loop
    static void
    request_cb(struct evhttp_request *req, void *arg);

    static void
    persist(evutil_socket_t fd, short what, void* args);

private :

    struct context{
        evhttp_request* req;
    };

    void process_request(struct evhttp_request *req);

    void persist_redis();

    void load_redis();

    void on_redis_loaded(
                std::shared_ptr<RTBKIT::Accounts> accounts,
                int status,
                const std::string & info);

    std::string
    get_command(struct evhttp_request *req);

    std::map<std::string, std::string>
    get_qs(const std::string& qs);

    std::string
    get_body(struct evbuffer *buf);

    struct event_base* base;

    std::shared_ptr<CarbonLogger> clog;

    Router router;

    RTBKIT::Accounts accounts;

    bool persisting;

    std::shared_ptr<Redis::AsyncConnection> redis;

    enum PersistenceCallbackStatus {
        SUCCESS,             /* info = "" */
        PERSISTENCE_ERROR,   /* info = error string */
        DATA_INCONSISTENCY   /* info = json array of account keys */
    };

    std::string
    create_error_msg(const std::string& m);

    void
    reactivatePresentAccounts(const RTBKIT::AccountKey & key);

    inline RTBKIT::AccountType rest_decode(const std::string & param){
        if (param == "none")
            return RTBKIT::AT_NONE;
        else if (param == "budget")
            return RTBKIT::AT_BUDGET;
        else if (param == "spend")
            return RTBKIT::AT_SPEND;
        else
            throw std::logic_error(create_error_msg("unknown account type " + param));
    }

    void restoreAccount(const RTBKIT::AccountKey & key);

};

}
#endif

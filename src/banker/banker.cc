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
#include <thread>
#include <memory>
#include <ostream>

const std::string PREFIX = "banker-";

MTX::MasterBanker::MasterBanker(
            struct event_base *base,
            std::shared_ptr<Redis::AsyncConnection> r):
                persisting(false), redis(r){
    LOG(INFO) << "building configuration ...";
    this->base = base;
}

MTX::MasterBanker::~MasterBanker(){
}

void MTX::MasterBanker::initialize(){
    //register the routes;

    Router::request_async_action adjustment = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("adjustment : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        Json::Value v = Json::parse(body);
        RTBKIT::Amount a("USD/1M", v["USD/1M"].asInt());
        RTBKIT::CurrencyPool amount(a);
        this->reactivatePresentAccounts(key);
        return this->accounts.addAdjustment(key, amount).toJson().toString();
    };

    Router::request_async_action balance = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("balance : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        Json::Value v = Json::parse(body);
        RTBKIT::Amount a("USD/1M", v["USD/1M"].asInt());
        RTBKIT::CurrencyPool newBalance(a);
        
        std::string acc_type;
        std::map<std::string, std::string>::const_iterator it;
        if((it = qs.find("accountType")) != qs.end())
            acc_type = it->second;
        if(!acc_type.size()){
            std::ostringstream msg;
            msg << "qs parameters required are accountType";
            throw std::logic_error(this->create_error_msg(msg.str()));
        }
        RTBKIT::AccountType t = this->rest_decode(acc_type);
        this->reactivatePresentAccounts(key);
        return this->accounts.setBalance(key, newBalance, t).toJson().toString();
    };

    Router::request_async_action shadow = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("shadow : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        Json::Value s_acc = Json::parse(body);
        RTBKIT::ShadowAccount sacc = RTBKIT::ShadowAccount::fromJson(s_acc);
        /*
        FIXME
        Record record(this, "syncFromShadow");
        */
        // ignore if account is closed.
        std::pair<bool, bool> presentActive =
                this->accounts.accountPresentAndActive(key);
        if (presentActive.first && !presentActive.second)
            return this->accounts.getAccount(key).toJson().toString();
        return this->accounts.syncFromShadow(key, sacc).toJson().toString();
    };


    Router::request_async_action budget = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("budget : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        RTBKIT::CurrencyPool budget;
        Json::Value v = Json::parse(body);
        RTBKIT::Amount a("USD/1M", v["USD/1M"].asInt());
        RTBKIT::CurrencyPool newBudget(a);
        reactivatePresentAccounts(key);
        return accounts.setBudget(key, newBudget).toJson().toString();
    };

    Router::request_async_action children = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("children : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        int depth = 0;
        std::map<std::string, std::string>::const_iterator it;
        if((it = qs.find("depth")) != qs.end())
            depth = atoi(it->second.c_str());
        std::vector<RTBKIT::AccountKey> keys;
        keys = this->accounts.getAccountKeys(key, depth);
        return Datacratic::jsonEncode(keys).toString();
    };

    Router::request_async_action close = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("close : " << path << " -> " << account_name);
        //FIXME nemi
        //Record record(this, "closeAccount");
        RTBKIT::AccountKey key(account_name);
        this->reactivatePresentAccounts(key);
        auto account = this->accounts.closeAccount(key);
        if (account.status == RTBKIT::Account::CLOSED)
            return "{\"message\":\"account was closed\"}";
        else{
            std::ostringstream msg;
            msg << "account could not be closed";
            throw std::logic_error(this->create_error_msg(msg.str()));
        }
    };

    Router::request_async_action subtree = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("subtree : " << path << " -> " << account_name);
        RTBKIT::AccountKey key(account_name);
        int depth = 0;
        std::map<std::string, std::string>::const_iterator it;
        if((it = qs.find("depth")) != qs.end())
            depth = atoi(it->second.c_str());
        RTBKIT::Accounts accs = this->accounts.getAccounts(key, depth);
        return accs.toJson().toString();
    };

    Router::request_async_action summary = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("summary : " << path << " -> " << account_name);
        if(account_name != "*" && account_name.size()){
            RTBKIT::AccountKey key(account_name);
            RTBKIT::AccountSummary s = this->accounts.getAccountSummary(key);
            return s.toJson().toString();
        }else if(account_name == "*"){
            int depth = 3;
            std::map<std::string, std::string>::const_iterator it;
            if((it = qs.find("depth")) != qs.end())
                depth = atoi(it->second.c_str());
            return accounts.getAccountSummariesJson(true, depth).toString();
        }
        std::ostringstream msg;
        msg << "error getting " << account_name;
        throw std::logic_error(this->create_error_msg(msg.str()));
    };

    Router::request_async_action accounts = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string {
        DLOGINFO("accounts : " << path << " -> " << account_name);
        if(account_name != "*" && account_name.size()){
            RTBKIT::Accounts::AccountInfo account =
                    this->accounts.getAccount(RTBKIT::AccountKey(account_name));
            return account.toJson().toString();
        }else if(account_name == "*"){
            std::vector<RTBKIT::AccountKey> keys =
                        this->accounts.getAccountKeys();
            return Datacratic::jsonEncode(keys).toString();
        }
        std::string msg = "{\"message\":\"Error getting ";
        msg += account_name;
        msg += "\"}";
        throw std::logic_error(msg);
    };

    Router::request_async_action create_account = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string {
        // get the qs params
        std::string acc_name, acc_type;
        std::map<std::string, std::string>::const_iterator it;
        if((it = qs.find("accountName")) != qs.end())
            acc_name = it->second;
        if((it = qs.find("accountType")) != qs.end())
            acc_type = it->second;
        if(!acc_name.size() || !acc_type.size()){
            std::ostringstream msg;
            msg << "qs parameters required are accountName and accountType";
            throw std::logic_error(this->create_error_msg(msg.str()));
        }
        RTBKIT::AccountKey k = RTBKIT::AccountKey(acc_name);
        RTBKIT::AccountType t = this->rest_decode(acc_type);
        reactivatePresentAccounts(k);
        return this->accounts.createAccount(k, t).toJson().toString();
    };

    Router::request_async_action active_accounts = [&](
                 const std::string& path,
                 const std::map<std::string, std::string>& qs,
                 const std::map<std::string, std::string>& headers,
                 const std::string& account_name,
                 const std::string& body) -> std::string{
        DLOGINFO("active accounts : " << path << " -> " << account_name);
        std::vector<RTBKIT::AccountKey> activeAccounts;
        auto addActive =
            [&activeAccounts] (const RTBKIT::AccountKey & ak, const RTBKIT::Account & a) {
                if (a.status == RTBKIT::Account::ACTIVE)
                    activeAccounts.push_back(ak);
            };
        this->accounts.forEachAccount(addActive);
        return Datacratic::jsonEncode(activeAccounts).toString();
    };

    // POST,PUT /v1/accounts/<accountName>/adjustment
    router.addAsyncRoute("POST", "adjustment", adjustment);
    router.addAsyncRoute("PUT", "adjustment", adjustment);
    // POST,PUT /v1/accounts/<accountName>/balance
    router.addAsyncRoute("POST", "balance", balance);
    router.addAsyncRoute("PUT", "balance", balance);
    // POST,PUT /v1/accounts/<accountName>/shadow
    router.addAsyncRoute("POST", "shadow", shadow);
    router.addAsyncRoute("PUT", "shadow", shadow);
    // POST,PUT /v1/accounts/<accountName>/budget
    router.addAsyncRoute("POST", "budget", budget);
    router.addAsyncRoute("PUT", "budget", budget);

    // GET /v1/accounts/<accountName>/children
    router.addAsyncRoute("GET", "children", children);
    // GET /v1/accounts/<accountName>/close
    router.addAsyncRoute("GET", "close", close);
    // GET /v1/accounts/<accountName>/subtree
    router.addAsyncRoute("GET", "subtree", subtree);
    // GET /v1/accounts/<accountName>/summary
    router.addAsyncRoute("GET", "summary", summary);

    // GET  /v1/accounts
    // GET  /v1/accounts/<accountName>
    router.addAsyncRoute("GET", "", accounts);

    // GET /v1/activeaccounts
    router.addAsyncRoute("GET", "activeaccounts", active_accounts);
    // POST /v1/accounts
    router.addAsyncRoute("POST", "", create_account);

    //load data from redis
    load_redis();
    
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

    try{
        std::string response_body;
        bool ok = router.route(cmdtype, path, qs_map, heads, body, response_body);
        if(ok){
            // set the response body
            struct evbuffer *evb = evbuffer_new();
            evbuffer_add_printf(evb, "%s", response_body.c_str());
            evhttp_add_header(evhttp_request_get_output_headers(req),
                                "Content-Type", "application/json");
            evhttp_add_header(evhttp_request_get_output_headers(req),
                                "Connection", "Keep-Alive");
            evhttp_send_reply(req, 200, "Ok", evb);
            evbuffer_free(evb);
        }else
            evhttp_send_reply(req, 404, "Not Found", NULL);
    }catch(ML::Exception& e){
        evhttp_send_reply(req, 404, "Not Found", NULL);
    }catch(std::logic_error& e){
        struct evbuffer *evb = evbuffer_new();
        evbuffer_add_printf(evb, "%s", e.what());
        evhttp_add_header(evhttp_request_get_output_headers(req),
                            "Content-Type", "application/json");
        evhttp_send_reply(req, 500, "ERROR", evb);
        evbuffer_free(evb);
    }catch(...){
        evhttp_send_reply(req, 500, "ERROR", NULL);
    }
}

void
MTX::MasterBanker::persist(evutil_socket_t fd, short what, void* args){
    MasterBanker* banker = (MasterBanker*)args;
    banker->persist_redis();
}


void
MTX::MasterBanker::persist_redis(){
    if(!persisting){
        persisting = true;
        std::thread t(
            [&](){
                try{
                    DLOGINFO("Persisting to redis");
                }catch(...){
                    LOG(ERROR) << "unkown error persisting";
                }
                persisting = false;
            }
        );
        t.detach();
    }else{
        LOG(WARNING) << "Persisting is taking too long!";
    }
}

void
MTX::MasterBanker::load_redis(){
    std::shared_ptr<RTBKIT::Accounts> newAccounts;

    Redis::Result result = redis->exec(Redis::SMEMBERS("banker:accounts"));
    if (!result.ok()) {
        on_redis_loaded(newAccounts, PERSISTENCE_ERROR, result.error());
        return;
    }

    const Redis::Reply & keysReply = result.reply();
    if (keysReply.type() != Redis:: ARRAY) {
        on_redis_loaded(newAccounts, DATA_INCONSISTENCY,
                 "SMEMBERS 'banker:accounts' must return an array");
        return;
    }

    newAccounts = std::make_shared<RTBKIT::Accounts>();
    if (keysReply.length() == 0) {
        on_redis_loaded(newAccounts, SUCCESS, "");
        return;
    }

    Redis::Command fetchCommand(Redis::MGET);
    std::vector<std::string> keys;
    for (int i = 0; i < keysReply.length(); i++) {
        std::string key(keysReply[i].asString());
        keys.push_back(key);
        fetchCommand.addArg(PREFIX + key);
    }

    result = redis->exec(fetchCommand);
    if (!result.ok()) {
        on_redis_loaded(newAccounts, PERSISTENCE_ERROR, result.error());
        return;
    }

    const Redis::Reply & accountsReply = result.reply();
    ExcAssert(accountsReply.type() == Redis::ARRAY);
    for (int i = 0; i < accountsReply.length(); i++) {
        if (accountsReply[i].type() == Redis::NIL) {
            on_redis_loaded(newAccounts, DATA_INCONSISTENCY,
                     "nil key '" + keys[i]
                     + "' referenced in 'banker:accounts'");
            return;
        }
        Json::Value storageValue = Json::parse(accountsReply[i]);
        newAccounts->restoreAccount(RTBKIT::AccountKey(keys[i]), storageValue);
    }

    on_redis_loaded(newAccounts, SUCCESS, "");
}

void
MTX::MasterBanker::on_redis_loaded(
                        std::shared_ptr<RTBKIT::Accounts> newAccounts,
                        int status,
                        const std::string & info){
    if (status == SUCCESS) {
        //recordHit("load.success");
        newAccounts->ensureInterAccountConsistency();
        accounts = *newAccounts;
        LOG(INFO) << "successfully loaded accounts";
    }
    else if (status == DATA_INCONSISTENCY) {
        //recordHit("load.inconsistencies");
        /* something is wrong with the backend data types */
        LOG(ERROR) << "Failed to load accounts, DATA_INCONSISTENCY: " << info;
    }
    else if (status == PERSISTENCE_ERROR) {
        //recordHit("load.error");
        /* the backend is unavailable */
        LOG(ERROR) << "Failed to load accounts, backend unavailable: " << info;
    }
    else {
        //recordHit("load.unknown");
        throw ML::Exception("status code is not handled");
    }
}
std::string
MTX::MasterBanker::create_error_msg(const std::string& m){
    std::string msg = "{\"message\":\"";
    msg += m;
    msg += "\"}";
    return msg;
}

void
MTX::MasterBanker::reactivatePresentAccounts(const RTBKIT::AccountKey & key) {
    std::pair<bool, bool> presentActive = accounts.accountPresentAndActive(key);
    if (!presentActive.first) {
        restoreAccount(key);
    }
    else if (presentActive.first && !presentActive.second) {
        accounts.reactivateAccount(key);
    }
}

void
MTX::MasterBanker::restoreAccount(const RTBKIT::AccountKey & key){
    //FIXME nemi
    //Record record(this, "restoreAttempt");

    std::pair<bool, bool> pAndA = accounts.accountPresentAndActive(key);
    if (pAndA.first && pAndA.second == RTBKIT::Account::CLOSED) {
        accounts.reactivateAccount(key);
        //recordHit("reactivated");
        return;
    } else if (pAndA.first && pAndA.second == RTBKIT::Account::ACTIVE) {
        //recordHit("alreadyActive");
        return;
    }
}

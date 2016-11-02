#include "router.h"
#include "utils/dlog.h"
#include <boost/algorithm/string.hpp>
#include <ostream>
#include <stdexcept>

MTX::Router::Router(const std::string& base_path){
    this->base_path = base_path;
}

MTX::Router::~Router(){}

void MTX::Router::addAsyncRoute(
        const std::string& method,
        const std::string& action,
        MTX::Router::request_async_action f){
    DLOGINFO("registering action : " << method << " " << action);
    if(method == "GET")
        get_actions.insert(std::make_pair(action, f));
    else if(method == "POST")
        post_actions.insert(std::make_pair(action, f));
    else if(method == "PUT")
        put_actions.insert(std::make_pair(action, f));
    else{
        std::string err = "addAsyncRoute:  unknow method ";
        err += method;
        throw std::logic_error(err);
    }
}

bool MTX::Router::route(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& qs,
        const std::map<std::string, std::string>& headers,
        const std::string& body){

    std::string action = "";
    std::string account = "*";
    std::string partial_path = "";

    if(!check_base_path(path)){
        //NASTY fix, but it's the fastest to do.
        if(path == "/v1/activeaccounts"){
            action = "activeaccounts";
        }else if(path == "/v1/summary"){
            action = "summary";
        }else{
            return false;
        }
    }else{
        //we know that base path is there, let's remove it
        partial_path = path.substr(base_path.size());
        DLOGINFO("partial path : [" << partial_path << "]");
    }

    if(partial_path.size()){
        std::vector<std::string> parts;
        boost::split(parts, partial_path, boost::is_any_of("/"));
        DLOGINFO("parts size : " << parts.size());
        if(parts.size() == 3){
            account = parts[1];
            action = parts[2];
        }else if(parts.size() == 2){
            account = parts[1];
        }else{
            return false;
        }
    }
    DLOGINFO("action : [" << action << "], account : [" << account << "]");
    std::map<std::string, request_async_action>::const_iterator it;
    if(method == "GET"){
        if((it = get_actions.find(action)) == get_actions.end()){
            return false;
        }
    }else if(method == "POST"){
        if((it = post_actions.find(action)) == post_actions.end()){
            return false;
        }
    }else if(method == "PUT"){
        if((it = put_actions.find(action)) == put_actions.end()){
            return false;
        }
    }else{
        return false;
    }

    it->second(path, qs, headers, account, body);
    return true;
}

bool MTX::Router::check_base_path(const std::string& path){
    if(path.find(base_path) == 0)
        return true;
    return false;
}

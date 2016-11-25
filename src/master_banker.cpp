#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#ifdef _EVENT_HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

#include "banker/banker.h"
#include "soa/service/redis.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <glog/logging.h>
#include <rapidjson/document.h>

#include <carboncxx/carbon_logger.h>
#include <carboncxx/carbon_connection_tcp.h>
#include <csignal>

// CLI paramters
DEFINE_int32(http_port, 7001, "Port to listen on with HTTP protocol");
DEFINE_string(ip, "0.0.0.0", "IP/Hostname to bind to");
DEFINE_string(redis_uri, "127.0.0.1:6379", "redis host:port");
DEFINE_int32(redis_db, 0, "Redis DB number");
DEFINE_int32(redis_dump_interval, 5, "Redis dump interval");
DEFINE_string(name, "MasterBanker", "Master banker name");
DEFINE_string(carbon_host, "127.0.0.1", "carbon host");
DEFINE_int32(carbon_port, 2003, "carbon port");

struct event_base *base;
std::shared_ptr<CarbonLogger> clog;
std::shared_ptr<MTX::MasterBanker> banker;

void signal_handler(int signal){
    LOG(WARNING) << "shutting down";
    event_base_loopbreak(base);
    banker->persist_redis();
    clog->stop_dumping_thread();
}

int
main(int argc, char **argv)
{
    struct evhttp *http;
    struct evhttp_bound_socket *handle;

    unsigned short port = 0;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    	return (1);

    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    base = event_base_new();
    if (!base) {
    	LOG(ERROR) << "Couldn't create an event_base: exiting";
    	return 1;
    }

    /* Create a new evhttp object to handle requests. */
    http = evhttp_new(base);
    if (!http) {
    	LOG(ERROR) << "couldn't create evhttp. Exiting.";
    	return 1;
    }

    auto address = Redis::Address(FLAGS_redis_uri);

    std::shared_ptr<Redis::AsyncConnection> redis;
    redis = std::make_shared<Redis::AsyncConnection>(FLAGS_redis_uri);
    if(FLAGS_redis_db != 0)
        redis->select(FLAGS_redis_db);
    redis->test();

    /* Start carbon loop in a separate thread */
    boost::asio::io_service ios;
    std::vector<std::shared_ptr<CarbonConnection>> cons;
    cons.push_back(std::make_shared<CarbonConnectionTCP>(
        FLAGS_carbon_host, FLAGS_carbon_port, ios));
    clog = std::make_shared<CarbonLogger>(FLAGS_name, cons);
    clog->init();
    clog->run_dumping_thread();

    /* Create the relay */
    banker = std::make_shared<MTX::MasterBanker>(base, redis, clog);
    banker->initialize();

    /* The callback */
    evhttp_set_gencb(http, MTX::MasterBanker::request_cb, banker.get());

    /* Now we tell the evhttp what port to listen on */
    handle = evhttp_bind_socket_with_handle(http,
                    FLAGS_ip.c_str(), (unsigned short)FLAGS_http_port);
    if (!handle) {
    	LOG(ERROR) << "couldn't bind to port " << port << ". Exiting.";
    	return 1;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    event* e = event_new(base, -1, EV_TIMEOUT | EV_PERSIST,
                                MTX::MasterBanker::persist, banker.get());
    timeval twoSec = {FLAGS_redis_dump_interval, 0};
    event_add(e, &twoSec);

    LOG(WARNING) << "Listening on " << FLAGS_ip <<
            ":" << FLAGS_http_port << " ...";
    event_base_dispatch(base);

    return 0;
}

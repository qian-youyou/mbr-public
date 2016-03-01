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

#include <iostream>
#include <sstream>
#include <glog/logging.h>

// CLI paramters
DEFINE_int32(http_port, 8989, "Port to listen on with HTTP protocol");
DEFINE_string(ip, "0.0.0.0", "IP/Hostname to bind to");
DEFINE_string(carbon_host, "127.0.0.1", "carbon host");
DEFINE_int32(carbon_port, 2003, "carbon port");
DEFINE_string(service_name, "http-augmentor",
             "service name used ad prefix for carbon metrics");

DEFINE_string(redis_host, "127.0.0.1", "redis host");
DEFINE_int32(redis_port, 6379, "redis port");
DEFINE_int32(redis_connections, 1, "redis connection pool size");

DEFINE_int32(acs_poll_interval, 2, "agent configuration polling interval");

DEFINE_string(ip_key_prefix, "coso:segment:ip", "redis key prefix for ip segments");
DEFINE_string(did_key_prefix, "coso:segment:did", "redis key prefix for did segments");

DEFINE_string(ip_augmentor_name, "ip-segment", "name of the ip augmentor");
DEFINE_string(did_augmentor_name, "did-segment", "name of the did augmentor");

DEFINE_string(augment_path, "/augment", "path for the augmentations");

int
main(int argc, char **argv)
{
    struct event_base *base;
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

    /* The /augment URI */
//    evhttp_set_cb(http, FLAGS_augment_path.c_str(),
//                    COSO::augment_request_cb, (void*)pool);

    /* Now we tell the evhttp what port to listen on */
    handle = evhttp_bind_socket_with_handle(http,
                    FLAGS_ip.c_str(), (unsigned short)FLAGS_http_port);
    if (!handle) {
    	LOG(ERROR) << "couldn't bind to port " << port << ". Exiting.";
    	return 1;
    }

    LOG(WARNING) << "Listening on " << FLAGS_ip <<
            ":" << FLAGS_http_port << " ...";
    event_base_dispatch(base);

    return 0;
}

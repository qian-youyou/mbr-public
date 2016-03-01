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

#include "relay/relay.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <glog/logging.h>
#include <rapidjson/document.h>

// CLI paramters
DEFINE_int32(http_port, 8989, "Port to listen on with HTTP protocol");
DEFINE_string(ip, "0.0.0.0", "IP/Hostname to bind to");

DEFINE_string(relay_config, "relay-config.json", "file with the relay configuration");

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

    /* open the config file and read it*/

    std::ifstream f(FLAGS_relay_config);
    std::string str((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    f.close();

    rapidjson::Document doc;
    doc.Parse(str.c_str());
    /* Create the relay */
    MTX::Relay rel(doc);

    /* The /v1/accounts URI */
    evhttp_set_cb(http, "/v1/accounts", rel.request_cb, &rel);

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

/*
 * http_connection_pool.h
 *
 *  Created on: Oct 31, 2016
 *      Author: pablin
 */

#ifndef SRC_UTILS_HTTP_CONNECTION_POOL_H_
#define SRC_UTILS_HTTP_CONNECTION_POOL_H_

#include <map>
#include <string>
#include <queue>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>


namespace MTX {

class HttpConnectionPool {

	/**
	 * @brief This class manage a group of connections to a particular server.
	 * Those connections are libevent ones.
	 * How to use :
	 * Create the class, get a connection, let the pool set appropiate header to
	 * the request being send, and return the connection to the pool:
	 *
	 * HttpConnectionPool pool(even_base, "localhost", 80);
	 * struct evhttp_connection* conn = get_connection();
	 *
	 * struct evhttp_request *req = (...)
	 * do some stuff with your request ...
	 * pool.set_connection_header(req, conn);
	 *
	 * Then, when you finish the request, just return the conn to the pool
	 * pool.return_connection(conn);
	 */

public:

	/**
	 * Creates the pool to the given host:port. Connections are not being opened
	 * until a connection is requested.
	 * @param base
	 * @param host
	 * @param port
	 */
	HttpConnectionPool(struct event_base* base, const std::string & host, int port);

	virtual ~HttpConnectionPool();

	/**
	 * @return a connection to the host:port.
	 */
	struct evhttp_connection* get_connection();

	/**
	 * You should return the connection after you make every http request.
	 * @param
	 */
	void return_connection(struct evhttp_connection*);

	/**
	 * @param a connection being handle by current HttpConnectionPool object.
	 * @return
	 */
	bool last_use(struct evhttp_connection*);

	/**
	 * You must call this method before sending the request to the server using
	 * a connection being handle by this pool.
	 * This will set the header Connection to keep-alive according to the
	 * set_request_before_recycling value (max_request_before_recycling).
	 *
	 * @param request that will be send using the given connection
	 * @param a connection handled by this pool
	 */
	void set_connection_header(struct evhttp_request *, struct evhttp_connection *);

	/**
	 * Connections are reuse by appending the Connection keep-alive header. This
	 * method, let you control how much requests are being performed before
	 * recycling the connection (closing it, and opening a new one).
	 * @param request_amount
	 */
	void set_requests_before_recycling(unsigned request_amount);

	/**
	 * Minimum amount of connections that the pool handles. Each time a connection
	 * is requested, a connection is created until this given amount of
	 * connections is reached.
	 * IMPORTANT : This is a minimum value, because if a connection if requested
	 * and no connections are available (the ones being used have not been returned)
	 * a new one gets created.
	 * @param upstream_connections
	 */
	void set_upstream_connections(unsigned upstream_connections);

private :

	// Host and port where the pool connection points to.
	std::string host;
	int port;

	// Keeps track of how many times a request have been requested.
	std::map< struct evhttp_connection* , unsigned > uses_per_conn;
	// Keeps the connections that are available ( the ones that have been returned).
	std::queue<struct evhttp_connection*> free_connections;

	// Max amount of requests per connection before closing it.
	unsigned max_request_before_recycling;
	// minimum amount of connection per upstream
	unsigned min_connections;

	struct event_base* ev_base;
};

}// end MTX

#endif /* SRC_UTILS_HTTP_CONNECTION_POOL_H_ */

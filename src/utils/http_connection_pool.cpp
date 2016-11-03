/*
 * http_connection_pool.cpp
 *
 *  Created on: Oct 31, 2016
 *      Author: pablin
 */

#include <utils/http_connection_pool.h>
#include <event2/http.h>


MTX::HttpConnectionPool::HttpConnectionPool(struct event_base* base, const std::string & host, int port)
:host(host), port(port), max_request_before_recycling(1000), min_connections(2), ev_base(base)
{

}

MTX::HttpConnectionPool::~HttpConnectionPool()
{
	while ( free_connections.size() > 0){
		evhttp_connection_free(free_connections.front());
		free_connections.pop();
	}
}

void
MTX::HttpConnectionPool::set_requests_before_recycling(unsigned request_amount)
{
	max_request_before_recycling = request_amount;
}

void
MTX::HttpConnectionPool::set_upstream_connections(unsigned upstream_connections)
{
	min_connections = upstream_connections;
}



struct evhttp_connection*
MTX::HttpConnectionPool::get_connection()
{
	struct evhttp_connection* conn = NULL;
	if ( free_connections.size() == 0 || uses_per_conn.size() < min_connections ){
		// If there is not an available connectiom, or the min poll size was
		// not reached, then create a new one
		conn = evhttp_connection_base_new(ev_base, NULL, host.c_str(), port);
		uses_per_conn[conn] = 1;
	} else {
		conn = free_connections.front();
		free_connections.pop();
		uses_per_conn[conn]++;
	}
	return conn;
}

void MTX::HttpConnectionPool::return_connection(struct evhttp_connection* conn)
{
	if ( last_use(conn)) {
		// if it was the last time the connection was used, then just free the connection
		uses_per_conn.erase(conn);
	    evhttp_connection_free(conn);
	} else {
		free_connections.push(conn);
	}
}

bool MTX::HttpConnectionPool::last_use(struct evhttp_connection* conn)
{
	return (uses_per_conn[conn] >= max_request_before_recycling);
}

void
MTX::HttpConnectionPool::
set_connection_header(struct evhttp_request * req, struct evhttp_connection * conn)
{
	if ( last_use(conn)){
		evhttp_add_header(req->output_headers, "Connection", "close");
	} else {
		evhttp_add_header(req->output_headers, "Connection", "keep-alive");
	}
}

std::string MTX::HttpConnectionPool::get_host() const
{
	return host;
}

int MTX::HttpConnectionPool::get_port() const
{
	return port;
}

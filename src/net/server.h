#pragma once

#include "types/error.h"

#include <netinet/in.h>

#define SERVER_MAX_ADDRESSES 32

// A description of a listen socket to be created.
typedef struct ListenAddress {
	// AF_INET, AF_INET6, ...
	int socket_family;

	// SOCK_STREAM
	int socket_type;

	struct sockaddr *addr;
	socklen_t addr_len;
} ListenAddress;

typedef struct ServerConnection {
	// This will be closed by `server_connection_deinit`.
	int fd;

	// The address of the connected client. This is an allocation that will be
	// freed by `server_connection_deinit`.
	struct sockaddr *client_addr;
	socklen_t client_addr_len;
} ServerConnection;

void server_connection_deinit(ServerConnection *self);

// A ServerAddress is the state for a single address that a Server is listening
// on.
typedef struct ServerAddress {
	int listen_fd;

	// Allocated separately.
	struct sockaddr *addr;
	socklen_t addr_len;
} ServerAddress;

// A Server is listening on multiple addresses, and may accept a connection from
// any of them.
//
// For simplicity, there's no API to stop listening on a port.
typedef struct Server {
	ServerAddress addresses[SERVER_MAX_ADDRESSES];
	size_t addresses_count;
} Server;

void server_init(Server *self);
void server_deinit(Server *self);

// Listen on `addr`. If this fails, an error is returned and the server is
// unaffected. All fields of `listen_address` are copied out and left unchanged.
Error server_listen(Server *self, ListenAddress listen_address);

// Accept a single connection from any of this servers' addresses. If this
// fails, `out_connection` is set to undefined and an error is returned.
// Otherwise, `out_connection` is a valid `ServerConnection` and must be
// deinitialized by calling `server_connection_deinit`.
Error server_accept(Server *self, ServerConnection *out_connection);

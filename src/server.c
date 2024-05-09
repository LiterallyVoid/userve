#include "server.h"

#include "util.h"

#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void server_connection_deinit(ServerConnection *self) {
	close(self->fd);

	free(self->server_addr);
	free(self->client_addr);

	set_undefined(self, sizeof(*self));
}

void server_init(Server *self) {
	set_undefined(self, sizeof(*self));

	self->addresses_count = 0;
}

void server_deinit(Server *self) {
	for (size_t i = 0; i < self->addresses_count; i++) {
		ServerAddress *address = &self->addresses[i];

		close(address->listen_fd);
		free(address->addr);
	}
	set_undefined(self, sizeof(*self));
}

static Error open_listen_socket(
	Server *self,
	ListenAddress listen_address,
	int *out_listen_fd
) {
	set_undefined(out_listen_fd, sizeof(*out_listen_fd));

	// In this function, `err` is a POSIX error, not an `Error` error.
	int err;

	int listen_fd = socket(
		listen_address.socket_family,
		listen_address.socket_type,
		0
	);
	if (listen_fd == -1) {
		perror("socket");
		return ERR_UNKNOWN;
	}

	// Enable REUSEADDR.
	int one = 1;
	err = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (err != 0) {
		perror("setsockopt");

		// Couldn't enable REUSEADDR. Continue anyway.
	}

	// Bind the socket to the specified address.
	err = bind(
		listen_fd,
		listen_address.addr,
		listen_address.addr_len
	);
	if (err != 0) {
		perror("bind");
		close(listen_fd);
		return ERR_UNKNOWN;
	}

	// Start listening.
	err = listen(
		listen_fd,

		// Kernel-side backlog buffer size
		32
	);
	if (err != 0) {
		perror("listen");
		close(listen_fd);
		return ERR_UNKNOWN;
	}

	*out_listen_fd = listen_fd;
	return ERR_SUCCESS;
}

Error server_listen(Server *self, ListenAddress listen_address) {
	if (self->addresses_count >= SERVER_MAX_ADDRESSES) {
		return ERR_OUT_OF_MEMORY;
	}

	Error err;

	int listen_fd;
	err = open_listen_socket(self, listen_address, &listen_fd);
	if (err != ERR_SUCCESS) return err;

	ServerAddress address;
	set_undefined(&address, sizeof(address));

	address.addr = malloc(listen_address.addr_len);
	if (address.addr == NULL) {
		close(listen_fd);
		return ERR_OUT_OF_MEMORY;
	}

	memcpy(address.addr, listen_address.addr, listen_address.addr_len);

	address.addr_len = listen_address.addr_len;

	self->addresses[self->addresses_count] = address;
	self->addresses_count += 1;

	return ERR_SUCCESS;
}

Error server_accept(Server *self, ServerConnection *out_connection) {
	set_undefined(out_connection, sizeof(*out_connection));

	struct pollfd pollfds[SERVER_MAX_ADDRESSES];
	set_undefined(pollfds, sizeof(pollfds));

	for (size_t i = 0; i < self->addresses_count; i++) {
		ServerAddress *address = &self->addresses[i];

		pollfds[i] = (struct pollfd) {
			.fd = address->listen_fd,
			.events = POLLIN,
			.revents = 0,
		};
	}

	int ready_count = poll(
		pollfds,
		self->addresses_count,
		-1
	);

	// Blocking server.
	if (ready_count <= 0) return ERR_UNKNOWN;
}

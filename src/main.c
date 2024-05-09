#include "http/parser.h"
#include "http/response.h"
#include "print.h"
#include "util.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

static Error respond_to_request(const HttpRequest *req, HttpResponse *res) {
	Error err;

	if (slice_equal(req->target, slice_from_cstr("/"))) {
		http_response_set_status(res, HTTP_OK);
		err = http_response_add_header(
			res,
			slice_from_cstr("Server"),
			slice_from_cstr("microserve")
		);
		if (err != ERR_SUCCESS) return err;

		http_response_end_with_body(
			res,
			slice_from_cstr("hey...?")
		);
		if (err != ERR_SUCCESS) return err;
	}

	return ERR_SUCCESS;
}

int main(int argc, char **argv) {
	struct addrinfo *listen_addresses = NULL;

	int err = getaddrinfo(
		"localhost",
		"3000",
		&(struct addrinfo) {
			.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = IPPROTO_TCP,
		},
		&listen_addresses
	);
	if (err != 0) {
		fprintf(stderr, "error retrieving address: %s", gai_strerror(err));
		return 1;
	}

	for (struct addrinfo *cursor = listen_addresses; cursor != NULL; cursor = cursor->ai_next) {
		printf("found address:\n");

		printf("  http://");
		print_address(stdout, cursor->ai_addr, cursor->ai_addrlen);
		printf("\n");
	}

	int server_fd = socket(
		listen_addresses->ai_family,
		listen_addresses->ai_socktype,
		0
	);
	if (server_fd == -1) {
		perror("socket");
		return 1;
	}

	int reuse_addr = 1;
	err = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	if (err != 0) {
		perror("setsockopt");
		return 1;
	}

	err = bind(
		server_fd,
		listen_addresses->ai_addr,
		listen_addresses->ai_addrlen
	);
	if (err != 0) {
		perror("bind");
		return 1;
	}
	freeaddrinfo(listen_addresses);

	err = listen(
		server_fd,

		// Kernel-side backlog
		32
	);
	if (err != 0) {
		perror("listen");
		return 1;
	}

	while (true) {
		struct sockaddr_in6 addr_in6 = { 0 };

		socklen_t addr_len = sizeof(addr_in6);
		struct sockaddr *header = (struct sockaddr*) &addr_in6;

		printf("Waiting for accept\n");
		int client_fd = accept(server_fd, header, &addr_len);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		printf("Got connection from ");
		print_address(stdout, header, addr_len);
		printf("!\n");

		HttpParser parser;
		http_parser_init(&parser);

		while (true) {
			uint8_t buffer[16];
			ssize_t recv_result = recv(client_fd, &buffer, sizeof(buffer), 0);
			if (recv_result == -1) {
				perror("read");
				break;
			}
			if (recv_result == 0) {
				break;
			}

			assert(recv_result >= 0);

			size_t buffer_len = recv_result;
			assert(buffer_len <= sizeof(buffer));

			HttpParserPollResult result;
			Error err = http_parser_poll(&parser, slice_from_len(buffer, buffer_len), &result);
			if (err != ERR_SUCCESS) {
				printf("error parsing request: %s\n", error_to_string(err));
				break;
			}

			if (result.status == HTTP_PARSER_INCOMPLETE) {
				printf("HTTP parsing incomplete\n");
				continue;
			} else if (result.status == HTTP_PARSER_DONE) {
				printf("HTTP request parsed, bytes remaining: %zu\n", result.done.remainder_slice.len);

				print_http_request(stdout, &result.done.request);

				HttpResponse response;
				http_response_init(&response, client_fd);

				respond_to_request(&result.done.request, &response);

				http_response_deinit(&response);
				http_request_deinit(&result.done.request);

				break;
			} else {
				assert(false);
			}
		}

		http_parser_deinit(&parser);
		close(client_fd);
	}
}

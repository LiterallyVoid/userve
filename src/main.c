#include "http/parser.h"
#include "http/response.h"
#include "print.h"
#include "server.h"
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

		err = http_response_end_with_body(
			res,
			slice_from_cstr("hey...?")
		);
		if (err != ERR_SUCCESS) return err;

		return ERR_SUCCESS;
	}

	err = http_response_not_found(res);
	if (err != ERR_SUCCESS) return err;

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

	Server server;
	server_init(&server);

	for (struct addrinfo *cursor = listen_addresses; cursor != NULL; cursor = cursor->ai_next) {
		printf("found address:\n");

		printf("  http://");
		print_address(stdout, cursor->ai_addr, cursor->ai_addrlen);
		printf("\n");

		Error err = server_listen(&server, (ListenAddress) {
			.socket_family = cursor->ai_family,
			.socket_type = cursor->ai_socktype,
			.addr = cursor->ai_addr,
			.addr_len = cursor->ai_addrlen,
		});
		if (err != ERR_SUCCESS) {
			printf("error listening to address: %s\n", error_to_string(err));
		}
	}

	while (true) {
		Error err;

		ServerConnection connection;
		err = server_accept(&server, &connection);
		if (err != ERR_SUCCESS) {
			printf("couldn't accept new connection: %s\n", error_to_string(err));
			continue;
		}

		printf("Got connection from ");
		print_address(stdout, connection.client_addr, connection.client_addr_len);
		printf("!\n");

		HttpParser parser;
		http_parser_init(&parser);

		while (true) {
			uint8_t buffer[512];
			ssize_t recv_result = recv(connection.fd, &buffer, sizeof(buffer), 0);
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
				http_response_init(&response, connection.fd);

				respond_to_request(&result.done.request, &response);

				http_response_deinit(&response);
				http_request_deinit(&result.done.request);

				break;
			} else {
				assert(false);
			}
		}

		http_parser_deinit(&parser);
		server_connection_deinit(&connection);
	}
}

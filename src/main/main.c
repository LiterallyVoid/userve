#include "http/parser.h"
#include "http/response.h"
#include "main/arguments.h"
#include "net/server.h"
#include "print.h"
#include "test/test.h"
#include "types/buffer.h"
#include "types/error.h"
#include "types/slice.h"
#include "util.h"
#include "types/hashmap.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <dirent.h>
#include <sys/stat.h>

static Slice detect_content_type(Slice path) {
	struct ContentType {
		Slice suffix;
		Slice content_type;
	} content_types[] = {
		// Text.
		{ slice_from_cstr(".txt"), slice_from_cstr("text/plain; charset=utf-8") },

		// HTML
		{ slice_from_cstr(".html"), slice_from_cstr("text/html; charset=utf-8") },
		{ slice_from_cstr(".css"), slice_from_cstr("text/css; charset=utf-8") },
		{ slice_from_cstr(".js"), slice_from_cstr("text/javascript; charset=utf-8") },

		// Documents?
		{ slice_from_cstr(".md"), slice_from_cstr("text/markdown; charset=utf-8") },
		{ slice_from_cstr(".pdf"), slice_from_cstr("application/pdf") },

		// Fonts
		{ slice_from_cstr(".ttf"), slice_from_cstr("font/ttf") },
		{ slice_from_cstr(".otf"), slice_from_cstr("font/otf") },
		{ slice_from_cstr(".woff"), slice_from_cstr("font/woff") },
		{ slice_from_cstr(".woff2"), slice_from_cstr("font/woff2") },

		// Multimedia
		// - Images
		{ slice_from_cstr(".png"), slice_from_cstr("image/png") },
		{ slice_from_cstr(".jpg"), slice_from_cstr("image/jpeg") },
		{ slice_from_cstr(".jpeg"), slice_from_cstr("image/jpeg") },
		{ slice_from_cstr(".svg"), slice_from_cstr("image/svg+xml") },
		{ slice_from_cstr(".ico"), slice_from_cstr("image/vnd.microsoft.icon") },

		// - Audio
		{ slice_from_cstr(".mp3"), slice_from_cstr("audio/mpeg") },
		{ slice_from_cstr(".ogg"), slice_from_cstr("application/ogg") },

		// - Video
		{ slice_from_cstr(".mp4"), slice_from_cstr("video/mp4") },
	};

	for (size_t i = 0; i < sizeof(content_types) / sizeof(content_types[0]); i++) {
		struct ContentType content_type = content_types[i];
		if (!slice_remove_suffix(&path, content_type.suffix)) continue;

		return content_type.content_type;
	}

	return slice_from_cstr("application/octet-stream");
}

typedef struct StaticFile {
	// Static memory.
	Slice content_type;

	Slice contents;
} StaticFile;

typedef struct StaticFileServer {
	// HashMap from owned URLs to StaticFile
	HashMap files;
} StaticFileServer;

static void staticfileserver_init(StaticFileServer *self) {
	set_undefined(self, sizeof(*self));

	hashmap_init(&self->files, sizeof(StaticFile));
}

static void staticfileserver_deinit(StaticFileServer *self) {
	HashMapIterator it = { 0 };

	HashMapEntry entry;
	while ((entry = hashmap_next(&self->files, &it)).occupied) {
		slice_free(*entry.key_ptr);

		StaticFile *file = (StaticFile*) entry.value_ptr;
		slice_free(file->content_type);
		slice_free(file->contents);
	}

	hashmap_deinit(&self->files);
}

static Error staticfileserver_load_file(
	StaticFileServer *self,
	const char *path,
	Slice url
) {
	Error err;

	FILE *fp = fopen(path, "rb");
	if (fp == NULL) return ERR_NOT_FOUND;

	Buffer file_contents;
	buffer_init(&file_contents);

	size_t chunk_size = 128;

	while (true) {
		err = buffer_reserve_additional(&file_contents, chunk_size);
		if (err != ERR_SUCCESS) {
			fclose(fp);
			buffer_deinit(&file_contents);
			return err;
		}

		Slice uninit = buffer_uninitialized(&file_contents);
		size_t read_amount = fread(uninit.bytes, 1, uninit.len, fp);
		if (read_amount == 0) break;

		// The last `read_amount` bytes of `file_contents` were written to by `fread`.
		// Update its length accordingly.
		file_contents.len += read_amount;

		chunk_size = read_amount * 2;
	}

	fclose(fp);

	// /path/index.html -> /path/
	slice_remove_suffix(&url, slice_from_cstr("index.html"));

	// /path.html -> /path
	slice_remove_suffix(&url, slice_from_cstr(".html"));

	HashMapEntry entry;
	err = hashmap_put(&self->files, url, &entry);
	if (err != ERR_SUCCESS) {
		buffer_deinit(&file_contents);
		return err;
	}

	assert(!entry.occupied);

	*entry.key_ptr = slice_clone(url);
	StaticFile *file = (StaticFile*) entry.value_ptr;

	file->content_type = detect_content_type(slice_from_cstr(path));
	file->contents = buffer_to_owned(&file_contents);

	return ERR_SUCCESS;
}

static Error staticfileserver_load_directory(
	StaticFileServer *self,
	const char *path,
	Slice url
) {
	Error err;

	// We don't take the `path` argument as a slice because it must be NUL-terminated.
	// This is an extra `strlen` per call, but that's acceptable.
	Slice path_slice = slice_from_cstr(path);

	DIR *dp = opendir(path);
	if (dp == NULL) return ERR_NOT_FOUND;

	struct dirent *dent;
	while ((dent = readdir(dp)) != NULL) {
		Slice entry_name = slice_from_cstr(dent->d_name);

		if (
			slice_equal(entry_name, slice_from_cstr(".")) ||
			slice_equal(entry_name, slice_from_cstr(".."))
		) {
			continue;
		}

		// Skip filenames that start with `.`
		if (entry_name.len > 0 && entry_name.bytes[0] == '.') continue;

		Buffer entry_path, entry_url;
		buffer_init(&entry_path);
		buffer_init(&entry_url);

		err = buffer_reserve_additional(
			&entry_path,
			path_slice.len
				// "/"
				+ 1
				+ entry_name.len
				// "\x00"
				+ 1 
		);
		if (err != ERR_SUCCESS) {
			closedir(dp);
			return err;
		}

		err = buffer_reserve_additional(
			&entry_url,
			url.len
				+ 1 /* "/" */
				+ entry_name.len
		);
		if (err != ERR_SUCCESS) {
			buffer_deinit(&entry_path);
			closedir(dp);
			return err;
		}

		buffer_concat_assume_capacity(&entry_path, path_slice);
		buffer_concat_assume_capacity(&entry_path, slice_from_cstr("/"));
		buffer_concat_assume_capacity(&entry_path, entry_name);
		// NUL-terminate `entry_path`.
		buffer_concat_assume_capacity(&entry_path, slice_from_len((uint8_t*) "\x00", 1));

		buffer_concat_assume_capacity(&entry_url, url);
		if (url.len > 0 && url.bytes[url.len - 1] != '/') {
			buffer_concat_assume_capacity(&entry_url, slice_from_cstr("/"));
		}
		buffer_concat_assume_capacity(&entry_url, entry_name);

		const char *entry_path_cstr = (const char*) entry_path.bytes;

		// @FIXME: there's a TOCTOU attack here!
		struct stat entry_stat;
		stat(entry_path_cstr, &entry_stat);

		if (entry_stat.st_mode & S_IFDIR) {
			staticfileserver_load_directory(
				self,
				entry_path_cstr,
				buffer_slice(&entry_url)
			);
		} else if (entry_stat.st_mode & S_IFREG) {
			staticfileserver_load_file(
				self,
				entry_path_cstr,
				buffer_slice(&entry_url)
			);
		}

		buffer_deinit(&entry_path);
		buffer_deinit(&entry_url);
	}

	closedir(dp);

	return ERR_SUCCESS;
}

// Returns `ERR_HTTP_NOT_FOUND` if `req.path` was not found.
static Error staticfileserver_respond(
	StaticFileServer *self,
	const HttpRequest *req,
	HttpResponse *res
) {
	Error err;

	// TODO: remove query parameters
	Slice path = req->target;

	HashMapEntry entry = hashmap_get(&self->files, path);
	if (!entry.occupied) return ERR_HTTP_NOT_FOUND;

	StaticFile *file = (StaticFile*) entry.value_ptr;

	http_response_set_status(res, HTTP_OK);
	err = http_response_add_header(
		res,
		slice_from_cstr("Content-Type"),
		file->content_type
	);
	if (err != ERR_SUCCESS) return err;

	err = http_response_end_with_body(res, file->contents);
	if (err != ERR_SUCCESS) return err;

	return ERR_SUCCESS;
}

int main(int argc, const char **argv) {
	Arguments arguments;
	arguments_parse(&arguments, argc, argv);

	if (arguments.test) {
		Error err = test_all();

		if (err == ERR_SUCCESS) return EXIT_SUCCESS;

		return EXIT_FAILURE;
	}

	if (arguments.fuzz) {
		fprintf(stderr, "todo: fuzzer\n");
		return 1;
	}

	struct addrinfo *listen_addresses = NULL;

	int err = getaddrinfo(
		arguments.address,
		arguments.port,
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
		Error err;

		// Try a handful of ports.
		for (int i = 0; i < 5; i++) {
			err = server_listen(&server, (ListenAddress) {
				.socket_family = cursor->ai_family,
				.socket_type = cursor->ai_socktype,
				.addr = cursor->ai_addr,
				.addr_len = cursor->ai_addrlen,
			});

			if (err == ERR_SUCCESS) break;

			printf(" failed to listen at http://");
			print_address(stdout, cursor->ai_addr, cursor->ai_addrlen);
			printf(": %s\n", error_to_string(err));

			uint16_t *port_raw;
			if (cursor->ai_addr->sa_family == AF_INET) {
				struct sockaddr_in *addr = (struct sockaddr_in*) cursor->ai_addr;
				port_raw = &addr->sin_port;
			} else if (cursor->ai_addr->sa_family == AF_INET6) {
				struct sockaddr_in6 *addr = (struct sockaddr_in6*) cursor->ai_addr;
				port_raw = &addr->sin6_port;
			} else {
				break;
			}

			uint16_t port = ntohs(*port_raw);
			if (port == 65535) break;

			*port_raw = htons(port + 1);
		}

		if (err == ERR_SUCCESS) {
			printf(" listening at http://");
			print_address(stdout, cursor->ai_addr, cursor->ai_addrlen);
			printf("\n");
		} else {
			printf("error listening to address: %s\n", error_to_string(err));
		}
	}

	StaticFileServer sfs;
	staticfileserver_init(&sfs);

	{
		Error err = staticfileserver_load_directory(&sfs, arguments.serve_path, slice_from_cstr("/"));
		if (err != ERR_SUCCESS) {
			printf("error loading static files from directory: %s\n", error_to_string(err));
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

		bool request_found = false;
		HttpRequest request;
		set_undefined(&request, sizeof(request));

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

			if (result.done) {
				request_found = true;
				request = result.request;
				break;
			}
		}

		if (request_found) {
			printf("got HTTP request:\n");
			print_http_request(stdout, &request);

			HttpResponse response;
			http_response_init(&response, connection.fd);

			err = staticfileserver_respond(&sfs, &request, &response);

			if (err != ERR_SUCCESS) {
				printf("error serving from staticfileserver: %s\n", error_to_string(err));

				if (err == ERR_HTTP_NOT_FOUND) {
					(void) http_response_not_found(&response);
				} else {
					(void) http_response_internal_server_error(&response);
				}
			}

			http_response_deinit(&response);
			http_request_deinit(&request);
		}

		http_parser_deinit(&parser);
		server_connection_deinit(&connection);
	}

	staticfileserver_deinit(&sfs);
}

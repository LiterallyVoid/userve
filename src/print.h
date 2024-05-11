#pragma once

#include "warble/buffer.h"
#include "http/request.h"

#include <netinet/in.h>
#include <stdio.h>

// Print a human-readable representation of socket address `addr` to `fp`.
void print_address(FILE *fp, struct sockaddr *addr, socklen_t addr_len);

// Print `slice`, with non-ASCII and control characters visible.
void print_slice(FILE *fp, Slice slice);
void print_http_request(FILE *fp, const HttpRequest *request);

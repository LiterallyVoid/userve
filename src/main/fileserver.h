#pragma once

#include "http/request.h"
#include "http/response.h"
#include "types/hashmap.h"
#include "types/slice.h"

typedef struct StaticFile {
	// Static memory.
	Slice content_type;

	Slice contents;
} StaticFile;

typedef struct FileServer {
	// HashMap from owned URLs to StaticFile
	HashMap files;
} FileServer;

void fileserver_init(FileServer *self);
void fileserver_deinit(FileServer *self);

Error fileserver_register_directory(
	FileServer *self,
	const char *path,
	Slice url
);

Error fileserver_respond(
	FileServer *self,
	const HttpRequest *req,
	HttpResponse *res
);

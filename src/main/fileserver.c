#include "main/fileserver.h"

#include "types/buffer.h"
#include "util.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void fileserver_init(FileServer *self) {
	set_undefined(self, sizeof(*self));

	hashmap_init(&self->files, sizeof(StaticFile));
}

void fileserver_deinit(FileServer *self) {
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

static Error fileserver_load_file(
	FileServer *self,
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

Error fileserver_register_directory(
	FileServer *self,
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

		if (S_ISDIR(entry_stat.st_mode)) {
			fileserver_register_directory(
				self,
				entry_path_cstr,
				buffer_slice(&entry_url)
			);
		} else if (S_ISREG(entry_stat.st_mode)) {
			fileserver_load_file(
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
Error fileserver_respond(
	FileServer *self,
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



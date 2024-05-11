VERSION = v0.2.0

OBJECTS = \
	src/http/parser.o	\
	src/http/request.o	\
	src/http/response.o	\
	src/main/arguments.o	\
	src/main/fileserver.o	\
	src/main/main.o	\
	src/print.o	\
	src/net/server.o	\
	src/util.o	\
	# end

OBJECTS += \
	src/test/test.o	\
	src/test/arguments.o	\
	src/test/http_parser.o

OBJECTS += \
	deps/warble/src/arraylist.o	\
	deps/warble/src/buffer.o	\
	deps/warble/src/error.o	\
	deps/warble/src/hash.o	\
	deps/warble/src/hashmap.o	\
	deps/warble/src/slice.o	\
	deps/warble/src/test.o	\
	deps/warble/src/util.o	\
	# end

CFLAGS = \
	-ftrivial-auto-var-init=pattern	\
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -ftrapv	\
	-DUSERVE_VERSION=\"$(VERSION)\" \
	-D_POSIX_C_SOURCE=200112L

INCLUDES = -Isrc/ -Ideps/warble/include/

LDFLAGS =

WARNINGS = -Wall -Wextra -Wmissing-prototypes -Wvla

EXE = userve

include deps/c-build/build.mk

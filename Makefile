VERSION = v0.2.0

OBJECTS = \
	src/common/hash.o	\
	src/types/buffer.o	\
	src/types/slice.o	\
	src/types/arraylist.o	\
	src/types/hashmap.o	\
	src/types/error.o	\
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
	src/test/harness.o	\
	src/test/test.o	\
	src/test/arguments.o	\
	src/test/buffer.o	\
	src/test/slice.o	\
	src/test/arraylist.o	\
	src/test/hashmap.o	\
	src/test/http_parser.o

CFLAGS = \
	-ftrivial-auto-var-init=pattern	\
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -ftrapv	\
	-DUSERVE_VERSION=\"$(VERSION)\" \
	-D_POSIX_C_SOURCE=200112L

INCLUDES = -Isrc/

LDFLAGS =

WARNINGS = -Wall -Wextra -Wmissing-prototypes -Wvla

EXE = userve

include deps/c-build/build.mk

OBJECTS = \
	src/buffer.o	\
	src/error.o	\
	src/http/parser.o	\
	src/http/request.o	\
	src/http/response.o	\
	src/main/arguments.o	\
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
	src/test/http_parser.o	\
	src/test/slice.o

CFLAGS = \
	-ftrivial-auto-var-init=pattern	\
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -ftrapv	\
	-DUSERVE_VERSION=\"v0.1.0\"

LDFLAGS =

WARNINGS = -Wall -Wextra -Wmissing-prototypes -Wvla

include deps/c-build/build.mk

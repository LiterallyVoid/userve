OBJECTS = \
	src/arraylist.o	\
	src/buffer.o	\
	src/error.o	\
	src/http/parser.o	\
	src/http/request.o	\
	src/http/response.o	\
	src/main.o	\
	src/print.o	\
	src/server.o \
	src/util.o \
	# end

CFLAGS = -pthread -ftrivial-auto-var-init=pattern -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -ftrapv
LDFLAGS = -pthread

WARNINGS = -Wall -Wextra -Wmissing-prototypes -Wvla

include deps/c-build/build.mk

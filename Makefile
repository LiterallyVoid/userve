OBJECTS = \
	src/arguments.o \
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

OBJECTS += \
	# test/test.c \
	# test/fuzz.c \
	# end

CFLAGS = -pthread -ftrivial-auto-var-init=pattern -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -ftrapv \
	-DUSERVE_VERSION=\"v0.1.0\"
LDFLAGS = -pthread

WARNINGS = -Wall -Wextra -Wmissing-prototypes -Wvla

include deps/c-build/build.mk

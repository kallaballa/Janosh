CXX     := g++
TARGET  := janosh
SRCS    := src/janosh.cpp src/tcp_server.cpp src/commands.cpp src/lua_script.cpp src/json.cpp src/websocket.cpp src/exception.cpp src/cache_thread.cpp src/exithandler.cpp src/value.cpp src/request.cpp src/logger.cpp src/cache.cpp src/flusher_thread.cpp src/path.cpp src/tcp_worker.cpp src/settings.cpp src/raw.cpp src/json_spirit/json_spirit_reader.cpp src/json_spirit/json_spirit_value.cpp src/json_spirit/json_spirit_writer.cpp src/tracker.cpp src/message_queue.cpp src/janosh_thread.cpp src/record.cpp src/backward.cpp src/bash.cpp src/tcp_client.cpp src/util.cpp src/database_thread.cpp src/component.cpp src/xdo.cpp
#precompiled headers
HEADERS :=  src/json_spirit/json_spirit.h
GCH     := ${HEADERS:.h=.gch}
OBJS    := ${SRCS:.cpp=.o}
DEPS    := ${SRCS:.cpp=.dep} 
DESTDIR := /
PREFIX  := /usr/local/
UNAME := $(shell uname)
export PATH := /usr/local/bin:$(PATH)

EXTRA_BUILDFLAGS = 

ifeq ($(UNAME), Linux)

#ifeq ($(shell uname -m), armv7l)
#  CXXFLAGS += -mfloat-abi=hard -mfpu=neon
#endif

CXXFLAGS += -Isrc/ -DWEBSOCKETPP_STRICT_MASKING -std=c++0x -pedantic -Wall -I./ -I/opt/local/include -D_XOPEN_SOURCE 
LDFLAGS += -Lluajit-rocks/build/luajit-2.0/ -L/opt/local/lib -Wl,--export-dynamic
LIBS    += -lboost_program_options -lboost_serialization -lboost_system -lboost_filesystem -lpthread -lboost_thread -lkyotocabinet -lluajit-5.1 -ldl -lzmq -lX11 -lxdo -lssl -lcrypto
endif

ifeq ($(UNAME), Darwin)
CXXFLAGS = -DJANOSH_NO_XDO -Isrc/ -DWEBSOCKETPP_STRICT_MASKING -Wall -I./luajit-rocks/luajit-2.0/src/ -I./ -I/opt/local/include -D_XOPEN_SOURCE -std=c++11 -stdlib=libc++ -pthread  -Wall -Wextra -pedantic
LIBS    := -lboost_program_options -lboost_serialization -lboost_system -lboost_filesystem -lpthread -lboost_thread-mt -lkyotocabinet -lluajit-5.1 -ldl -lzmq -lssl -lcrypto
EXTRA_BUILDFLAGS = -pagezero_size 10000 -image_base 100000000
#Darwin - we use clang
CXX := clang++
HEADERS := 
GCH :=
endif

ifeq ($(CXX), clang++)
CXXFLAGS+=-Wgnu-zero-variadic-macro-arguments -Wthread-safety
else
CXXFLAGS+=-rdynamic
endif

.PHONY: all release static clean distclean 

all: release

release: LDFLAGS += -Wl,--export-dynamic -s
release: CXXFLAGS += -g0 -O3
release: LIBS += -ldw 
release: ${TARGET}

reduce: CXXFLAGS = -DWEBSOCKETPP_STRICT_MASKING -DETLOG -std=c++0x -pedantic -Wall -g0 -Os -fvisibility=hidden -fvisibility-inlines-hidden
reduce: ${TARGET}

static: LDFLAGS += -s
static: CXXFLAGS += -g0 -O3
static: LIBS = -Wl,-Bstatic -lboost_serialization -lboost_program_options -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lluajit-5.1 -lzmq -lX11 -lxdo -lssl -lcrypto
static: ${TARGET}

screeninvader: LDFLAGS += -s
screeninvader: CXXFLAGS += -D_JANOSH_DEBUG -g0 -O3 
screeninvader: LIBS = -lboost_program_options -Wl,-Bstatic -lboost_serialization -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lluajit-5.1 -lzmq -lX11 -lxdo -ldw -lssl -lcrypto
screeninvader: ${TARGET}

screeninvader_debug: LDFLAGS += -Wl,--export-dynamic
screeninvader_debug: CXXFLAGS += -D_JANOSH_DEBUG -g3 -O0 -rdynamic
screeninvader_debug: LIBS = -lboost_program_options -Wl,-Bstatic -lboost_serialization -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lbfd -lluajit-5.1 -lzmq -lX11 -lxdo -ldw -lssl -lcrypto
screeninvader_debug: ${TARGET}

debug: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG
debug: LDFLAGS += -Wl,--export-dynamic
debug: LIBS+= -lbfd -ldl -ldw
debug: ${TARGET}

asan: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG -fno-omit-frame-pointer -fsanitize=address
asan: LDFLAGS += -Wl,--export-dynamic -fsanitize=address
asan: LIBS+= -lbfd -ldw
asan: ${TARGET}

src/JanoshAPI.o:	src/JanoshAPI.lua
	luajit -b src/JanoshAPI.lua src/JanoshAPI.o
src/JSONLib.o:	src/JSONLib.lua
	luajit -b src/JSONLib.lua src/JSONLib.o
src/Web.o:  src/Web.lua
	luajit -b src/Web.lua src/Web.o


${TARGET}: ${OBJS} src/JSONLib.o src/JanoshAPI.o src/Web.o
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} ${EXTRA_BUILDFLAGS}

${OBJS}: %.o: %.cpp %.dep ${GCH}
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

${GCH}: %.gch: ${HEADERS} 
	${CXX} ${CXXFLAGS} -o $@ -c ${@:.gch=.h}

install:
	mkdir -p ${DESTDIR}/${PREFIX}/bin
	cp ${TARGET} ${DESTDIR}/${PREFIX}/bin

uninstall:
	rm ${DESTDIR}/${PREFIX}/${TARGET}

clean:
	rm -f *~ ${DEPS} ${OBJS} ${GCH} ${TARGET} src/JanoshAPI.o src/JSONLib.o

distclean: clean




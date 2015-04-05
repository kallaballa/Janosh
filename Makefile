CXX     := g++
TARGET  := janosh
SRCS    := janosh.cpp logger.cpp record.cpp path.cpp value.cpp exception.cpp cache.cpp json_spirit/json_spirit_reader.cpp  json_spirit/json_spirit_value.cpp  json_spirit/json_spirit_writer.cpp tcp_server.cpp tcp_client.cpp janosh_thread.cpp commands.cpp settings.cpp request.cpp tracker.cpp backward.cpp component.cpp json.cpp bash.cpp raw.cpp util.cpp exithandler.cpp cache_thread.cpp database_thread.cpp flusher_thread.cpp tcp_worker.cpp lua_script.cpp websocket.cpp message_queue.cpp
#precompiled headers
HEADERS := backward.h easylogging++.h json_spirit/json_spirit.h
GCH     := ${HEADERS:.h=.gch}
OBJS    := ${SRCS:.cpp=.o}
DEPS    := ${SRCS:.cpp=.dep} 
DESTDIR := /
PREFIX  := /usr/local/
UNAME := $(shell uname)
export PATH := /usr/local/bin:$(PATH)

EXTRA_BUILDFLAGS = 

ifeq ($(UNAME), Linux)

ifeq ($(shell uname -m), armv7l)
  CXXFLAGS += -mfloat-abi=hard -mfpu=neon
endif

CXXFLAGS += -DWEBSOCKETPP_STRICT_MASKING -DETLOG -std=c++0x -pedantic -Wall -I./websocketpp/ -I./backtrace/ -I/opt/local/include -DELPP_THREAD_SAFE  -DELPP_DISABLE_LOGGING_FLAGS_FROM_ARG -DELPP_DISABLE_DEFAULT_CRASH_HANDLING -DELPP_NO_DEFAULT_LOG_FILE -D_XOPEN_SOURCE 
LDFLAGS += -Lluajit-rocks/build/luajit-2.0/ -L/opt/local/lib -s
LIBS    += -lboost_program_options -lboost_serialization -lboost_system -lboost_filesystem -lpthread -lboost_thread -lkyotocabinet -lluajit-5.1 -ldl -lzmq
endif

ifeq ($(UNAME), Darwin)
CXXFLAGS = -DWEBSOCKETPP_STRICT_MASKING -DETLOG -Wall -I./luajit-rocks/luajit-2.0/src/ -I./websocketpp/ -I./backtrace/ -I/opt/local/include -DELPP_DEBUG_ERRORS -DELPP_THREAD_SAFE -DELPP_STL_LOGGING -DELPP_LOG_UNORDERED_SET -DELPP_LOG_UNORDERED_MAP -DELPP_STACKTRACE_ON_CRASH -DELPP_LOGGING_FLAGS_FROM_ARG -D_XOPEN_SOURCE -std=c++11 -stdlib=libc++ -pthread  -Wall -Wextra -pedantic
LIBS    := -lboost_program_options -lboost_serialization -lboost_system -lboost_filesystem -lpthread -lboost_thread-mt -lkyotocabinet -lluajit-5.1 -ldl -lzmq
EXTRA_BUILDFLAGS = -pagezero_size 10000 -image_base 100000000
#Darwin - we use clang
CXX := clang++
HEADERS := 
GCH :=
endif

.PHONY: all release static clean distclean 

all: release

#release: LDFLAGS += -s
#release: CXXFLAGS += -g0 -O3 -D_ELPP_DISABLE_DEBUG_LOGS
release: ${TARGET}

reduce: CXXFLAGS = -DWEBSOCKETPP_STRICT_MASKING -DETLOG -std=c++0x -pedantic -Wall -I./backtrace/ -g0 -Os -fvisibility=hidden -fvisibility-inlines-hidden
reduce: ${TARGET}

static: LDFLAGS += -s
static: CXXFLAGS += -g0 -O3
static: LIBS = -Wl,-Bstatic -lboost_serialization -lboost_program_options -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lluajit-5.1 -lzmq
static: ${TARGET}

screeninvader: LDFLAGS += -s
screeninvader: CXXFLAGS += -D_JANOSH_DEBUG -g0 -O3 
screeninvader: LIBS = -lboost_program_options -Wl,-Bstatic -lboost_serialization -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lluajit-5.1 -lzmq
screeninvader: ${TARGET}

screeninvader_debug: LDFLAGS += -Wl,--export-dynamic
screeninvader_debug: CXXFLAGS += -D_JANOSH_DEBUG -g3 -O0 -rdynamic -D_JANOSH_DEBUG
screeninvader_debug: LIBS = -lboost_program_options -Wl,-Bstatic -lboost_serialization -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lbfd -lluajit-5.1 -lzmq
screeninvader_debug: ${TARGET}

debug: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG
debug: LDFLAGS += -Wl,--export-dynamic
debug: LIBS+= -lbfd
debug: ${TARGET}

asan: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG -fno-omit-frame-pointer -fsanitize=address
asan: LDFLAGS += -Wl,--export-dynamic -fsanitize=address
asan: LIBS+= -lbfd
asan: ${TARGET}

JanoshAPI.o:	JanoshAPI.lua
	luajit -b JanoshAPI.lua JanoshAPI.o
JSONLib.o:	JSONLib.lua
	luajit -b JSONLib.lua JSONLib.o

${TARGET}: ${OBJS} JSONLib.o JanoshAPI.o 
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
	rm -f *~ ${DEPS} ${OBJS} ${GCH} ${TARGET} JanoshAPI.o JSONLib.o

distclean: clean




CXX     := g++
TARGET  := janosh
SRCS    := janosh.cpp logger.cpp record.cpp path.cpp value.cpp exception.cpp cache.cpp json_spirit/json_spirit_reader.cpp  json_spirit/json_spirit_value.cpp  json_spirit/json_spirit_writer.cpp tcp_server.cpp tcp_client.cpp janosh_thread.cpp commands.cpp settings.cpp request.cpp tracker.cpp backward.cpp component.cpp json.cpp bash.cpp raw.cpp util.cpp exithandler.cpp cache_thread.cpp database_thread.cpp flusher_thread.cpp tcp_worker.cpp lua_script.cpp websocket.cpp message_queue.cpp
#precompiled headers
HEADERS := backward.h easylogging++.h json_spirit/json_spirit.h
LUAFILES:= JSON.lua JanoshAPI.lua
GCH     := ${HEADERS:.h=.gch}
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
LUAOBJS := ${LUAFILES:.lua=.o} 
DESTDIR := /
PREFIX  := /usr/local/

CXXFLAGS += -DWEBSOCKETPP_STRICT_MASKING -DETLOG -std=c++0x -pedantic -Wall -I./luajit-rocks/luajit-2.0/src/ -I./websocketpp/ -I./backtrace/ -I/opt/local/include -D_ELPP_THREAD_SAFE  -D_ELPP_DISABLE_LOGGING_FLAGS_FROM_ARG -D_ELPP_DISABLE_DEFAULT_CRASH_HANDLING -D_ELPP_NO_DEFAULT_LOG_FILE
LDFLAGS += -L/opt/local/lib
LIBS    += -lboost_program_options -lboost_serialization -lboost_system -lboost_filesystem -lpthread -lboost_thread -lkyotocabinet -lluajit-5.1 -ldl -lzmq

BINOUTPUT := elf64-x86-64
BINARCH := i386

ifeq ($(shell uname -m), armv7l)
  BINOUTPUT = elf32-littlearm
  BINARCH = arm
  CXXFLAGS += -mfloat-abi=hard -mfpu=neon
  
endif
.PHONY: all release static clean distclean 

ifeq ($(UNAME), Darwin)
 CXXFLAGS +=  -stdlib=libc++
endif

all: release

release: LDFLAGS += -s
release: CXXFLAGS += -g0 -O3 -D_ELPP_DISABLE_DEBUG_LOGS
release: ${TARGET}

reduce: CXXFLAGS = -DWEBSOCKETPP_STRICT_MASKING -DETLOG -std=c++0x -pedantic -Wall -I./backtrace/ -g0 -Os -fvisibility=hidden -fvisibility-inlines-hidden
reduce: ${TARGET}

static: LDFLAGS += -s
static: CXXFLAGS += -g0 -O3
static: LIBS = -Wl,-Bstatic -lboost_serialization -lboost_program_options -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lluajit-5.1 -lzmq
static: ${TARGET}

screeninvader: LDFLAGS += -s
screeninvader: CXXFLAGS += -D_JANOSH_DEBUG -g0 -O3 
screeninvader: LIBS = -Wl,-Bstatic -lboost_serialization -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lboost_program_options -lluajit-5.1 -lzmq
screeninvader: ${TARGET}

screeninvader_debug: LDFLAGS += -Wl,--export-dynamic
screeninvader_debug: CXXFLAGS += -D_JANOSH_DEBUG -g3 -O0 -rdynamic -D_JANOSH_DEBUG
screeninvader_debug: LIBS = -Wl,-Bstatic -lboost_serialization -lboost_program_options -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl -lbfd -lluajit-5.1 -lzmq
screeninvader_debug: ${TARGET}

debug: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG
debug: LDFLAGS += -Wl,--export-dynamic
debug: LIBS+= -lbfd
debug: ${TARGET}

asan: CXXFLAGS += -g3 -O0 -rdynamic -D_JANOSH_DEBUG -fno-omit-frame-pointer -fsanitize=address
asan: LDFLAGS += -Wl,--export-dynamic -fsanitize=address
asan: LIBS+= -lbfd
asan: ${TARGET}

${TARGET}: ${LUAOBJS} ${OBJS}
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${LUAOBJS}: %.o: ${LUAFILES} 
	objcopy --input binary --output ${BINOUTPUT} --binary-architecture ${BINARCH} ${@:.o=.lua} $@

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
	rm -f *~ ${DEPS} ${OBJS} ${GCH} ${TARGET} 

distclean: clean




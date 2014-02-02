CXX     := g++-4.8
TARGET  := janosh 
SRCS    := janosh.cpp logger.cpp tri_logger/tri_logger.cpp record.cpp path.cpp value.cpp backtrace/libs/backtrace/src/backtrace.cpp json_spirit/json_spirit_reader.cpp  json_spirit/json_spirit_value.cpp  json_spirit/json_spirit_writer.cpp TcpServer.cpp TcpClient.cpp janosh_thread.cpp commands.cpp
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
    
CXXFLAGS = -DETLOG -std=c++0x -pedantic -Wall -I./backtrace/ -I/opt/local/include
LDFLAGS = -L/opt/local/lib 
LIBS    = -lboost_system-mt -lboost_filesystem-mt -lpthread -lboost_thread-mt -lkyotocabinet  -ldl

.PHONY: all release static clean distclean 

ifeq ($(UNAME), Darwin)
 CXXFLAGS +=  -stdlib=libc++
endif

all: release

release: LDFLAGS += -s
release: CXXFLAGS += -g0 -O3
release: ${TARGET}

reduce: CXXFLAGS = -DETLOG -std=c++0x -pedantic -Wall -I./backtrace/ -g0 -Os -fvisibility=hidden -fvisibility-inlines-hidden
reduce: ${TARGET}

static: LDFLAGS += -s
static: CXXFLAGS += -g0 -O3
static: LIBS = -Wl,-Bstatic -lboost_system -lboost_filesystem -lkyotocabinet  -llzma -llzo2 -Wl,-Bdynamic -lz -lpthread -lrt -ldl
static: ${TARGET}

debug: CXXFLAGS += -DJANOSH_DEBUG -g3 -O0 -rdynamic
debug: LDFLAGS += -Wl,--export-dynamic
debug: ${TARGET}

${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

clean:
	rm -f *~ *.dep *.o backtrace/libs/backtrace/src/*.dep backtrace/libs/backtrace/src/*.o tri_logger/*.dep tri_logger/*.o json_spirit/*.dep json_spirit/*.o ${TARGET} 

distclean: clean


TARGET  := janosh 
SRCS    := janosh.cpp logger.cpp tri_logger/tri_logger.cpp record.cpp channel.cpp path.cpp value.cpp backtrace/libs/backtrace/src/backtrace.cpp
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
    
CXXFLAGS = -DETLOG -std=c++0x -pedantic -Wall -I/usr/local/include/
LDFLAGS = -L/usr/lib64/
LIBS    = -lboost_system -lboost_filesystem -ljson_spirit -lpthread -lboost_thread -lkyotocabinet  -lrt -ldl

.PHONY: all release static clean distclean 

ifdef DEBUG
 CXXFLAGS += -DJANOSH_DEBUG -g3 -O0 -rdynamic -I./backtrace/
 LDFLAGS += -Wl,--export-dynamic
else
 LDFLAGS += -s
 CXFLAGS += -g0 -03
endif

all: release

release: ${TARGET}

static: CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden -I./backtrace/
static: LIBS = -Wl,-Bstatic -lboost_system -lboost_filesystem -ljson_spirit -lboost_thread -Wl,-Bdynamic -lkyotocabinet -lpthread -lrt -ldl
static: ${TARGET}

${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

clean:
	rm -f *~ *.o backtrace/libs/backtrace/src/*.o ${TARGET} 

distclean: clean


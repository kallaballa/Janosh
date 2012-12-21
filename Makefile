TARGET  := janosh 
SRCS    := janosh.cpp logger.cpp tri_logger/tri_logger.cpp record.cpp channel.cpp path.cpp value.cpp backtrace/libs/backtrace/src/backtrace.cpp
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
    
CXXFLAGS = -DETLOG -std=c++0x -pedantic -Wall -I/usr/local/include/
LDFLAGS = -s
LDDEBUGFLAGS = -export-dynamic
LIBS    = -L/usr/lib64/ -lboost_system -lboost_filesystem -ljson_spirit -lpthread -lboost_thread -lkyotocabinet  -lrt -ldl

.PHONY: all debug clean distclean 

all: debug

release: CXXFLAGS += -g0 -O3 
release: ${TARGET}

debug: CXXFLAGS += -DJANOSH_DEBUG -g3 -O0 -rdynamic -I./backtrace/
debug: ${TARGET}

${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

clean:
	rm -f *~ *.o backtrace/libs/backtrace/src/*.o ${TARGET} 

distclean: clean


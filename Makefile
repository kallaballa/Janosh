    TARGET  := janosh 
    SRCS    := janosh.cpp Logger.cpp tri_logger/tri_logger.cpp record.cpp
    OBJS    := ${SRCS:.cpp=.o} 
    DEPS    := ${SRCS:.cpp=.dep} 

CXXFLAGS = -march=native -std=c++0x -I/usr/local/include/ -Os -Wall -DETLOG
    LDFLAGS = -s 
    LIBS    = -L/usr/lib64/ -lboost_system -lboost_filesystem -ljson_spirit -lpthread -lboost_thread -lkyotocabinet  -lrt

    .PHONY: all clean distclean 
    all:: ${TARGET} 

    ifneq (${XDEPS},) 
    include ${XDEPS} 
    endif 

    ${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

    ${OBJS}: %.o: %.cpp %.dep 
	${CXX} ${CXXFLAGS} -o $@ -c $< 

    ${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

    clean:: 
	-rm -f *~ *.o ${TARGET} 

    distclean:: clean


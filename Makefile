    TARGET  := janosh 
    SRCS    := janosh.cpp
    OBJS    := ${SRCS:.cpp=.o} 
    DEPS    := ${SRCS:.cpp=.dep} 

    CXXFLAGS = -std=c++0x -I/usr/local/include/ -O3 -Wall 
    LDFLAGS = -s 
    LIBS    = -lboost_system -lboost_filesystem -lkyotocabinet -L/usr/local/lib  -rdynamic /usr/local/lib/libjson_spirit.a

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


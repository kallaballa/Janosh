    TARGET  := janosh 
    SRCS    := janosh.cpp
    OBJS    := ${SRCS:.cpp=.o} 
    DEPS    := ${SRCS:.cpp=.dep} 

    CXXFLAGS = -lkyotocabinet -ljson_spirit -I/usr/local/include/ -L/usr/local/lib -O3 -Wall 
    LDFLAGS = -s 
    LIBS    = -lkyotocabinet -L/usr/local/lib  -rdynamic /usr/local/lib/libjson_spirit.a

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


all: parser

OBJS = 	parser.o \
       	main.o \
       	tokens.o \
	codegen.o \
	corefn.o \
	compile.o \
	types.o \
	runtime.o \
	SplitFuncs.o

CPPFLAGS = `llvm-config-3.4 --cppflags` -std=c++11 -Wall
LDFLAGS = `llvm-config-3.4 --ldflags`
LIBS = `llvm-config-3.4 --libs`

clean:
	$(RM) -rf parser.cpp parser.hpp parser tokens.cpp $(OBJS)

parser.cpp: parser.y node.h 
	bison -d -o $@ $<

parser.hpp: parser.cpp

tokens.cpp: tokens.l parser.hpp
	flex -o $@ $^

%.o: %.cpp node.h codegen.h runtime.h
	g++ -c $(CPPFLAGS) -o $@ $<


parser: $(OBJS)
	g++ -o $@ $(OBJS) $(LIBS) $(LDFLAGS)




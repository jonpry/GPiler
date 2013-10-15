all: parser

OBJS = 	parser.o \
       	main.o \
       	tokens.o \
	codegen.o \
	corefn.o \
	compile.o

CPPFLAGS = `llvm-config-3.1 --cppflags`
LDFLAGS = `llvm-config-3.1 --ldflags`
LIBS = `llvm-config-3.1 --libs`

clean:
	$(RM) -rf parser.cpp parser.hpp parser tokens.cpp $(OBJS)

parser.cpp: parser.y node.h 
	bison -d -o $@ $<

parser.hpp: parser.cpp

tokens.cpp: tokens.l parser.hpp
	flex -o $@ $^

%.o: %.cpp node.h
	g++ -c $(CPPFLAGS) -o $@ $<


parser: $(OBJS)
	g++ -o $@ $(OBJS) $(LIBS) $(LDFLAGS)




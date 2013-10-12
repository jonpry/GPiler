all: parser

OBJS = 	parser.o \
       	main.o \
       	tokens.o \
	codegen.o 

CPPFLAGS = `llvm-config-3.0 --cppflags`
LDFLAGS = `llvm-config-3.0 --ldflags`
LIBS = `llvm-config-3.0 --libs`

clean:
	$(RM) -rf parser.cpp parser.hpp parser tokens.cpp $(OBJS)

parser.cpp: parser.y
	bison -d -o $@ $^

parser.hpp: parser.cpp

tokens.cpp: tokens.l parser.hpp
	flex -o $@ $^

%.o: %.cpp
	g++ -c $(CPPFLAGS) -o $@ $<


parser: $(OBJS)
	g++ -o $@ $(OBJS) $(LIBS) $(LDFLAGS)




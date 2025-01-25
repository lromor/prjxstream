CXXFLAGS=-std=c++17 -fno-exceptions -fno-rtti -W -Wall -Wextra -pedantic -O3
CFLAGS=-std=c99 -W -Wall -Wextra -pedantic -Wno-unused-parameter -O3

BINARIES=fasm-parse_test fasm-validation-parse c-fasm-validation-parse \
         fasm-generate-testfile

all: $(BINARIES)

test: fasm-parse_test
	./fasm-parse_test

fasm-parse_test.o: fasm-parse.h

c-fasm-validation-parse.o: c-fasm-parse.h
c-fasm-validation-parse: c-fasm-validation-parse.o c-fasm-parse.o
	$(CC) -o $@ $^

fasm-validation-parse.o: fasm-parse.h
fasm-validation-parse: fasm-validation-parse.o
	$(CXX) -o $@ $^ -lpthread

c-fasm-parse.o: c-fasm-parse.h fasm-parse.h
% : %.o
	$(CXX) -o $@ $^

# Add these so that we have tab-completion in make
$(BINARIES):

clean:
	rm -f *.o $(BINARIES)

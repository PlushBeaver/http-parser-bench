CC = g++
CFLAGS = -std=c++14 -O2 -I./gsl/include
LDFLAGS = -lhttp_parser -lpion -lboost_system -llog4cpp -lcrypto

all: http-parser-bench

http-parser-bench: http-parser-bench.cc
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

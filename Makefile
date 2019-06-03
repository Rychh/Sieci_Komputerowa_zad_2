TARGET: netstore-client netstore-server

CXX = g++
FLAGS = -std=c++17 -Wall -Werror -O2
BOOST = -lboost_program_options -lboost_system -lboost_filesystem -lpthread 


all: netstore-server netstore-client


netstore-server: server.cpp
	$(CXX) $(FLAGS) $< $(BOOST) -o $@ helper.cpp
netstore-client: client.cpp
	$(CXX) $(FLAGS) $< $(BOOST) -o $@ helper.cpp

.PHONY: clean TARGET
clean:
	rm  -f  *.o netstore-client netstore-server

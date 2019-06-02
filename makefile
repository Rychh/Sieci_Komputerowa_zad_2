TARGET: server client



server:
	g++ -L/usr/bin -lboost_program_options -lboost_filesystem -lboost_system -o server server.cpp

client: client.cpp helper.cpp
	g++ -L/usr/bin -lboost_program_options -lboost_filesystem -lboost_system -o client client.cc helper.cpp


.PHONY: clean TARGET
clean:
	rm -f server client *.o *~ *.bak


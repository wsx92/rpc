DYNAMIC_LINKINGS=-lssl -lgflags
CC=gcc
CXX=g++
CXXFLAG=-std=c++11

SERVER_SOURCES = http.cpp iobuf.cpp
SERVER_OBJS = $(addsuffix .o, $(basename $(SERVER_SOURCES)))

.PHONY:all
all: http_server

.PHONY:clean
clean:
	@echo "Cleaning"
	@rm -rf http_server $(SERVER_OBJS)

http_server:$(SERVER_OBJS)
	@echo "Linking $@"
	@$(CXX) $(SERVER_OBJS) $(DYNAMIC_LINKINGS) -o $@
%.o:%.cpp
	@echo "Compiling $@"
	@$(CXX) -c $(CXXFLAG)  $< -o $@

CC=gcc
CXX=g++

CXXFLAG=-std=c++11 -g
LINKINGS=-lssl -lgtest -lgtest_main

TEST_SOURCES = iobuf.cpp iobuf_unittest.cpp
TEST_OBJS = $(addsuffix .o, $(basename $(TEST_SOURCES)))


.PHONY:all
all: test

.PHONY:clean
clean:
	@echo "Cleaning"
	@rm -rf test $(TEST_OBJS)

test:$(TEST_OBJS)
	@echo "Linking $@"
	@$(CXX) $(TEST_OBJS) $(LINKINGS) -o $@
%.o:%.cpp
	@echo "Compiling $@"
	@$(CXX) -c $(CXXFLAG)  $< -o $@


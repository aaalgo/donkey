.PHONY:	all clean

PROGS = server client 
all:	$(PROGS)

clean:
	rm -rf *.o $(PROGS) grpc thrift *.tag

include plugin/Makefile.inc
include Makefile.protocol.inc

CXXFLAGS += -std=c++11 -O3 -g $(EXTRA_CXXFLAGS) -Igrpc -Ithrift -I../3rd/FastEMD -I$(PWD) -DBOOST_LOG_DYN_LINK
LDFLAGS += $(EXTRA_LDFLAGS)
LDLIBS += -lkgraph $(PROTOCOL_LIBS) -lboost_timer -lboost_chrono -lboost_program_options -lboost_log -lboost_log_setup -lboost_thread -lboost_filesystem -lboost_system $(EXTRA_LIBS) -lpthread -lrt -ldl

X_HEADERS = $(patsubst %.h, plugin/%.h, $(EXTRA_HEADERS))
X_OBJS = $(patsubst %.cpp, plugin/%.o, $(EXTRA_SOURCES))
X_OBJS = $(patsubst %.c, plugin/%.o, $(X_OBJS))

HEADERS = donkey.h plugin/config.h $(X_HEADERS) 

TAGS = protocol.tag
SERVER_OBJS = server.o donkey.o index-kgraph.o index-lsh.o $(PROTOCOL_OBJS) $(X_OBJS)
CLIENT_OBJS = client.o donkey.o $(PROTOCOL_OBJS) $(X_OBJS)

server:	$(TAGS) $(SERVER_OBJS) $(HEADERS)  
	echo $(EXTRA_SOURCES)
	echo $(X_OBJS)
	$(CXX) $(LDFLAGS) $(SERVER_OBJS) $(LDLIBS) -o $@ 

client:	$(TAGS) $(CLIENT_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(CLIENT_OBJS) $(LDLIBS) -o $@ 

%.o:	%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $*.o

build-info:	build-info.cpp $(HEADERS)
	$(CXX) $(LDFLAGS) build-info.cpp $(LDLIBS) -o $@


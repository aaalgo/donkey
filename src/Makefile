.PHONY:	all clean

PROGS = server client proxy stress
all:	$(PROGS)

clean:
	rm -rf *.o $(PROGS) grpc thrift *.tag

include plugin/Makefile.inc
include Makefile.thrift.inc

CXXFLAGS += -fopenmp -std=c++11 -O3 -g $(EXTRA_CXXFLAGS) -Iplugin -Igrpc -Ithrift -I../3rd/FastEMD -I$(PWD) $(shell ./check_boost.sh) #-DBOOST_LOG_DYN_LINK
LDFLAGS += -fopenmp $(EXTRA_LDFLAGS)
LDLIBS += -lkgraph $(PROTOCOL_LIBS) -lboost_timer -lboost_chrono -lboost_program_options -lboost_log -lboost_log_setup -lboost_thread -lboost_filesystem -lboost_system $(EXTRA_LIBS) -lpthread -lrt -ldl $(EXTRA_EXTRA_LIBS)

X_HEADERS = $(patsubst %.h, plugin/%.h, $(EXTRA_HEADERS))
X_OBJS1 = $(patsubst %.cpp, plugin/%.o, $(EXTRA_SOURCES))
X_OBJS = $(patsubst %.c, plugin/%.o, $(X_OBJS1))

HEADERS = donkey.h plugin/config.h $(X_HEADERS) 

TAGS = protocol.tag
SERVER_OBJS = server.o donkey.o logging.o index-kgraph.o index-lsh.o $(PROTOCOL_OBJS) $(X_OBJS)
CLIENT_OBJS = client.o donkey.o logging.o index-kgraph.o index-lsh.o $(PROTOCOL_OBJS) $(X_OBJS)
PROXY_OBJS = proxy.o donkey.o logging.o $(PROTOCOL_OBJS) $(X_OBJS)
STRESS_OBJS = stress.o donkey.o logging.o $(PROTOCOL_OBJS) $(X_OBJS)

server:	$(TAGS) $(SERVER_OBJS) $(HEADERS)  
	echo $(EXTRA_SOURCES)
	echo $(X_OBJS)
	$(CXX) $(LDFLAGS) $(SERVER_OBJS) $(LDLIBS) -o $@ 

client:	$(TAGS) $(CLIENT_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(CLIENT_OBJS) $(LDLIBS) -o $@ 

stress:	$(TAGS) $(STRESS_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(STRESS_OBJS) $(LDLIBS) -o $@ 

proxy:	$(TAGS) $(PROXY_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(PROXY_OBJS) $(LDLIBS) -o $@ 

%.o:	%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $*.o

build-info:	build-info.cpp $(HEADERS)
	$(CXX) $(LDFLAGS) build-info.cpp $(LDLIBS) -o $@


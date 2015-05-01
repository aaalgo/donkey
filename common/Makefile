include Makefile.protocol.inc

CXXFLAGS += -std=c++11 -O3 -g $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)
LDLIBS += $(PROTOCOL_LIBS) -lprotobuf -lboost_timer -lboost_chrono -lboost_program_options -lboost_log -lboost_thread -lboost_system $(EXTRA_LDLIBS) -lpthread -lrt -ldl

X_HEADERS = $(patsubst %.h, plugin/%.h, $(EXTRA_HEADERS))
X_OBJS = $(patsubst %.cpp, plugin/%.o, $(EXTRA_SOURCES))

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
PROTO_PATH = plugin

vpath %.proto $(PROTO_PATH)

PROGS = server client

.PHONY:	all clean

all:	$(PROGS)

clean:
	rm -f *.o $(PROGS) *.pb.*

HEADERS = donkey.h $(X_HEADERS)

SERVER_OBJS = server.o object.pb.o $(PROTO_SERVER_OBJS)
CLIENT_OBJS = client.o $(PROTO_CLIENT_OBJS)

server:	$(SERVER_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(SERVER_OBJS) $(LDLIBS) -o $@ 

client:	$(CLIENT_OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) $(SERVER_OBJS) $(LDLIBS) -o $@ 

%.grpc.pb.cc:	%.proto
        $(PROTOC) -I $(PROTO_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<
	$(PROTOC) -I $(PROTO_PATH) --cpp_out=. $<

%.pb.cc:	%.proto
	$(PROTOC) -I $(PROTO_PATH) --cpp_out=. $<


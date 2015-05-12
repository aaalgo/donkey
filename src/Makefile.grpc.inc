PROTOCOL_LIBS = -lgrpc++_unsecure -lgrpc -lgpr -lprotobuf
PROTOCOL_OBJS = donkey-grpc.o grpc/donkey.grpc.pb.o grpc/donkey.pb.o

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

protocol.tag:	donkey.proto
	mkdir -p grpc
	protoc --grpc_out=grpc --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) donkey.proto
	protoc --cpp_out=grpc donkey.proto
	touch protocol.tag


PROTOCOL_LIBS = -lthrift
PROTOCOL_OBJS = thrift/donkey_constants.o  thrift/Donkey.o thrift/donkey_types.o donkey-thrift.o

protocol.tag:	donkey.thrift
	mkdir -p thrift
	thrift -gen cpp:include_prefix -out thrift donkey.thrift
	touch protocol.tag


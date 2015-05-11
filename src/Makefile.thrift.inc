PROTOCOL_LIBS = -lthrift
PROTOCOL_OBJS = thrift/donkey_constants.o  thrift/Donkey.o thrift/donkey_types.o

protocol.tag:	donkey.thrift
	mkdir -p thrift
	thrift -out thrift -gen cpp donkey.thrift
	touch protocol.tag


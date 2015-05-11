.PHONY:	all clean nuke

all:
	./heehaw qbic

clean:
	make -C src clean
	rm -f src/plugin
	rm -f src/Makefile.protocol.inc
	find . -name '*.o' -exec rm {} \;
	find . -name '*~' -exec rm {} \;

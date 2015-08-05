# donkey

Wei Dong
wdong@wdong.org

1. Overview

DONKEY is a toolkit for generating content-based search engines (QBSE).
A content-based search engine indexes a collection of objects, and when presented
with query objects, quickly returns objects that are most similar to the query.

One example is content-based image retrieval, where search requests are submitted with
a query image rather than keywords.

http://en.wikipedia.org/wiki/Content-based_image_retrieval

Another example is query by humming, where the user submits a recorded clip of
humming, and the search engine returns the closest song or music.

http://en.wikipedia.org/wiki/Query_by_humming

A QBSE can be viewed as a key-value store, that searches key by value with approximate
matching allowed.

QBSEs differ in that each type of object (image, audio, text, ...) has its specific
internal representation (called feature vectors), and its specific similarity measure.
What's common to all QBSE is that they all need a database to efficiently manage and
search the feature data.  It is the goal of DONKEY to provide a common QBSE
infrastructure.  The user can generate a QBSE by plugging in:

- A data type definition of feature vectors.
- A similarity function to compare feature vectors.
- An algorithm to convert data objects to feature vectors.
- Other customization code.

DONKEY makes design choice to be generic and easy to use. In order to
meet these criteria, it has to make a number of compromizes:

- It is not for super massive datasets.  The maximal dataset size is at the scale of
millions of feature vectors.
- It is not for super fast retrieval.  Rather, we use algorithms that are fast for
all kinds of data.
- It has limited flexibility.  Although a donkey server can manage multiple object
collections, all have to be of the same data type.

2. Workflow

In order to generate a search engine for a specific data type, the user has to create a
directory and provide at least two files:
- config.h: specifying the configuration.
- Makefile
- (Optional) extra C++ and C source files.

Examples can be found in the "examples" directory.


To generate the search engine for the "image" example, 
run:

$ cd examples/image
$ make

The server and client binaries will be generated.

The server can be configured with a configuration file, which controls the server behavior
like port, parallelism, logging, etc.  Once the server is up and running, the user can
use the client-side API or the client binary to submit search requests, and inject data objects
into the search engine.


3. API

DONKEY supports Thrift API.  Other protocols like gRPC and HTTP are planned.

3.1. Thrift


4. Plugin Development Guide

4.1 Makefile

Following is a minimal sample Makefile


DONKEY_HOME=$(HOME)/src/donkey	# wherever donkey home is

EXTRA_CXXFLAGS = -I..	# add extra flags
EXTRA_LDFLAGS = -L..
EXTRA_LIBS = -lz	# add extra libs

EXTRA_C_SOURCES = xxx.c
EXTRA_SOURCES = xxx.cpp

include $(DONKEY_HOME)/src/Makefile.common


What's mandatory is to define DONKEY_HOME, and include the Makefile.common file.


4.2 Object Definition

4.3 C++ functions



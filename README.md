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
sub-directory under the plugin directory, and provide at least three files:
- config.sh: specifying the configuration.
- object.proto: define the data types for feature and meta data.
- a C++ file implements the feature extraction, similarity search and ranking
functions.

An example is provided under the name qbic.  To generate the search engine for qbic,
run:

$ ./heehaw qbic

And a server named donkey-qbic (defined in qbic/config.sh) is generated.

The server can be configured with a configuration file, which controls the server behavior
like port, parallelism, logging, etc.  Once the server is up and running, the user can
use the client-side API to submit search requests, and inject data objects into the
search engine.


3. API

DONKEY provides two options for server API, http and gRPC, which are configurable in plugin.

3.1. Thrift


4. Plugin Development Guide

4.1 Configuration

4.2 Object Definition

4.3 C++ functions




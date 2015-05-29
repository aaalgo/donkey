Building instruction:

The code builds under Ubuntu 14.04 with g++ 4.8 and with the following libraries installed:

- boost (libboost1.55-all, but 1.54 should also do).
- OpenCV.
- FLANN (not needed now, but we'll soon integrate it.)
- libevent-dev and libssl-dev (required by Thrift).

The above three can be installed with the following command:
  apt-get install libboost1.55-all libopencv-dev libflann-dev libevent-dev libssl-dev

- Thrift 
- ProtoBuf + gRPC (optional).
- KGraph (Download from www.kgraph.org, manually put kgraph.h and libkgraph.so in system directories.) The dependency will be removed later.


After the source is cloned, run under project root

./heehaw qbic

The source tree will be configured to build with the qbic plugin and thrift.  After that, incremental build can be done by running "make" under the src directory.


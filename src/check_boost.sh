#!/bin/bash

# http://www.boost.org/doc/libs/1_54_0/libs/log/doc/html/log/rationale/namespace_mangling.html
#
# Namespace mangling may lead to linking errors if the application is
# misconfigured. One common mistake is to build dynamic version of the library
# and not define BOOST_LOG_DYN_LINK or BOOST_ALL_DYN_LINK when building the
# application, so that the library assumes static linking by default. Whenever
# such linking errors appear, one can decode the namespace name in the missing
# symbols and the exported symbols of Boost.Log library and adjust library or
# application configuration accordingly.

# try to find shared boost library, and print g++ flag accordingly

g++ -print-search-dirs | grep '^libraries' | sed 's/^libraries: =//' | sed 's/:/\n/g' | while read DIR
do
    if [ -f $DIR/libboost_log_setup.so ]
    then
        echo -DBOOST_LOG_DYN_LINK
        exit
    fi
done




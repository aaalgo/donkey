A web demo of donkey.
##requirement
+ npm
+ thrift

##install
./configure.sh

##run
+ node(nodejs) thriftServer.js &

	will run thrift server for test

+ ./run.sh

	will start the web server

##url
1. GET host:port/ 		main page
2. POST host:port/search/file	ajax api for searching content
3. GET host:port/search/url	ajax api for searching url

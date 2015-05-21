var path= require('path')
var fs= require('fs')
var config={};


config.port=8081;
config.rootDir=__dirname;
config.viewsDir=path.join(config.rootDir,'views');

config.thriftPort=9090;

config.apiOptions={
    searchCacheSize:1000,
    numberOfItems:20
};


module.exports=config;

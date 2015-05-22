var path= require('path')
var fs= require('fs')
var config={};


config.rootDir=__dirname;
config.viewsDir=path.join(config.rootDir,'views');

config.port=38081;
config.thriftPort=38082;
config.rootPath="/qbic/";

config.apiOptions={
    searchCacheSize:1000,
    numberOfItems:20
};


module.exports=config;

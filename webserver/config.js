var path= require('path')
var fs= require('fs')
var config={};


config.port=8080;
config.rootDir=__dirname;
config.viewsDir=path.join(config.rootDir,'views');
config.default_timezone='-5.0';

config.apiOptions={
    searchCacheSize:1000,
    numberOfItems:20
};


module.exports=config;

//This module provide search function
//Include connection to thrift server, pagination, and search caches.

var thrift = require('thrift');
var ThriftTransports = require('./node_modules/thrift/lib/thrift/transport');
var ThriftProtocols = require('./node_modules/thrift/lib/thrift/protocol');
var config = require('./config');
var Donkey = require('./gen-nodejs/Donkey');
var ttypes = require('./gen-nodejs/donkey_types');
var LRU = require("lru-cache")
//module object
var mod={};

//store previous search result
var searchCache = LRU(config.apiOptions.searchCacheSize); // sets just the max size
searchCache.newId=function(){
    this.cur=this.cur+1||1;
    while(searchCache.has(this.cur)){
        this.cur=this.cur+1;
    }
    return this.cur;
}


//Manage thrift connection and client
ClientManager={
    transport : ThriftTransports.TBufferedTransport(),
    protocol : ThriftProtocols.TBinaryProtocol(),
    //create connection and client
    createConn:function(){
        var conn= thrift.createConnection("localhost", config.thriftPort, {
            transport : this.transport,protocol : this.protocol
        });
        //error handle
        //reconnect if connection is broken
        conn.on('error', function(err) {
            console.error(err.stack);
            ClientManager.createConn();
            client=ClientManager.getClient();
        });
        this.client=thrift.createClient(Donkey,conn);
    },
    getClient:function(){
        if(this.client==null){
            this.createConn();
        }
        return this.client;
    }

}
client=ClientManager.getClient();
ping=new ttypes.PingRequest();
//check connect is valid every config.thriftPingInterval
setInterval(function(){
    client.ping(ping,function(err,res){
    })
},config.thriftPingInterval);



//handle search request from router.js
//retrieve search option from req
//retrieve search content from fetchContent
//return using callback
function search(req,callback,fetchContent){
    var id=req.query.query_id;
    var result=searchCache.get(id);
    var info={id:id};
	if(info.id==null){
		info.id=searchCache.newId();
	}
    //not in searchCache
    if(result==null){
        var q=new ttypes.SearchRequest();
        q.db=0;
        q.raw=true;
        q.type="";
        q.url="";
		q.content="";
		fetchContent(q,thriftSearch(paginate(req,callback),info),info);
	}
	//in searchCache
	else{
		paginate(req,callback)(null,result,info);
	}
}
mod.search=search;


//return a function would be called after search request for thrift is prepared
//process thrift request and respond with callback
function thriftSearch(callback,info){
	return function(err,request){
		if(err!=null){
			console.error(err.stack);
			callback(err);
		}else{
			client.search(request,function(err,response){
				if(err==null){
					for(i in response.hits){
						response.hits[i].meta=JSON.parse(response.hits[i].meta);
						response.hits[i].meta.thumb=config.rootPath+response.hits[i].meta.thumb;
					}
					searchCache.set(info.id,response);
					callback(null,response,info);
				}else{
					callback(err);

				}
			});
		}
	}
}


//return a function slicing result into a page according to information in req and info
//req store information from GET parameter
//info store information after search
//return resultPage using respond function
function paginate(req,respond){
	return function(err,result,info){
		if(err){
			console.error(err.stack);
			respond(err);
			return;
		}
		var numOfItems=req.query.num_of_items-0;
		if(numOfItems==null||isNaN(numOfItems)||numOfItems<1){
			numOfItems=config.apiOptions.numberOfItems;
		}

		info.numOfPages=Math.floor((result.hits.length-1)/numOfItems+1);

		info.pageNum=req.query.page-0;
		if(info.pageNum==null||isNaN(info.pageNum)||info.pageNum<0){
			info.pageNum=0;
		}

		var resultPage={
			time:result.time,
			load_time:result.load_time,
			filter_time:result.filter_time,
			rank_time:result.rank_time,
			hits:result.hits.slice(info.pageNum*numOfItems,(info.pageNum+1)*numOfItems)
		}
		var json={
			query_id:info.id,
			pageNum:info.pageNum,
			numOfPages: info.numOfPages,
			data:resultPage
		};
		respond(json);
	}
}

module.exports = mod

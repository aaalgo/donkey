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
            console.log("thrift connection error");
            console.log(err.stack);
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


function search(req,callback,fetchContent){
    var id=req.query.query_id;
    var result=searchCache.get(id);
    var info={id:id};
    //not in searchCache
    if(result==null){
        var q=new ttypes.SearchRequest();
        q.db=0;
        q.raw=true;
        q.type="";
        q.url="";
        q.content="";
        console.log("begin fetch");

        var fet=fetchContent(q,function(fet){
            console.log("fetch"+ fet)
            if(fet!=null){
                console.log("fetch failed");
                callback(fet);
                return;
            };
        client.search(q,function(err,response){
            if(err==null){
                var id=searchCache.newId();
                searchCache.set(id,response);
                info.id=id;
                for(i in response.hits){
                    response.hits[i].meta=JSON.parse(response.hits[i].meta);
                    response.hits[i].meta.thumb=config.rootPath+response.hits[i].meta.thumb;
                }
                paginate(req,response,info,searchRespond(callback));
            }else{
                callback(err);

            }});

        });
    //in searchCache
    }else{
        paginate(req,result,info,searchRespond(callback));
    }
}
mod.search=search;

function paginate(req,result,info,respond){
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
    respond(resultPage,info);
}

function searchRespond(callback){
    return function(result,info){
        var json={
            query_id:info.id,
            pageNum:info.pageNum,
            numOfPages: info.numOfPages,
            data:result
        };
        callback(json);
    }
}
module.exports = mod

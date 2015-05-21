var thrift = require('thrift');
var config = require('./config');
var thrift = require('thrift');
var Donkey = require('./gen-nodejs/Donkey');
var ttypes = require('./gen-nodejs/donkey_types');
var LRU = require("lru-cache")
var searchCache = LRU(config.apiOptions.searchCacheSize); // sets just the max size
var mod={};

searchCache.newId=function(){
    this.cur=this.cur+1||1;
    while(searchCache.has(this.cur)){
        this.cur=this.cur+1;
    }
    return this.cur;
}

var connection = thrift.createConnection("localhost", config.thriftPort);

connection.on('error', function(err) {
	console.log(err.stack);
});

// Create a Calculator client with the connection
var client = thrift.createClient(Donkey, connection);


function search(req,callback,fetchContent){
    var id=req.query.query_id;
    var result=searchCache.get(id);
    var info={id:id};
    //not in LRU
    if(result==null){
	var q=new ttypes.SearchRequest();
	q.db=0;
	q.raw=true;
	q.type="";
	q.url=null;
	q.content=null;

        fetchContent(q);
	console.log("@client : "+JSON.stringify(q));
        client.search(q,function(err,response){
            if(err==null){
                var id=searchCache.newId();
                searchCache.set(id,response);
                info.id=id;
        	paginate(req,response,info,searchRespond(callback));
            }else{
callback(err);
}
        });
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

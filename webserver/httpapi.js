var express = require('express');
var thrift = require('thrift');
var moment = require('moment');
var config = require('./config');
var thrift = require('thrift');
var Donkey = require('./gen-nodejs/Donkey');
var ttypes = require('./gen-nodejs/donkey_types');
var LRU = require("lru-cache")
var searchCache = LRU(config.apiOptions.searchCacheSize); // sets just the max size

searchCache.newId=function(){
    this.cur=this.cur+1||1;
    while(LRU.has(this.cur)){
        this.cur=this.cur+1;
    }
    return this.cur;
}

var connection = thrift.createConnection("localhost", config.thriftIp
        );

connection.on('error', function(err) {
    assert(false, err);
});

// Create a Calculator client with the connection
var client = thrift.createClient(Calculator, connection);


router.get('/local', function(req, res) {
    search(req,searchRespond(res),function(q){
        q.content=req.params.content;
    })
});
router.get('/write', function(req, res) {
    search(req,searchRespond(res),function(q){
        q.url=req.params.url;
    })
});

function search(req,respond,fetchContent){
    var id=req.params.query_id;
    var result=searchCache.get(id);
    var info={id:id};
    //not in LRU
    if(result==null){
        var q={
            db:0,
            raw:true,
            type:"",
            url:null,
            content:null
        }
        fetchContent(q);
        client.search(q,function(err,response){
            if(err==null){
                var id=newId();
                searchCache.set(id,response);
                info.id=id;
                paginate(req,response,info,respond); 
            }
        });
    }else{
        paginate(req,result,info,respond);
    }
}

function paginate(req,result,info,respond){
    var numOfItems=req.params.num_of_items;
    if(numOfItems==null||isNan(numOfItems-0)||numOfItems<1){
        numOfItems=config.apiOptions.numberOfItems;
    }

    info.numOfPages=Math.floor((result.hits.length-1)/numOfItems+1);

    info.pageNum=req.params.page;
    if(info.pageNum==null||isNan(info.pageNum-0)||info.pageNum<0){
        info.pageNum=0;
    }

    resultPage={
        time:result.time,
        load_time:result.load_time,
        filter_time:result.filter_time,
        rank_time:result.rank_time,
        hits:hits.slice(info.pageNum*numOfItems,(info.pageNum+1)*numOfItems)
    }
    respond(resultPage,info);
}

function searchRespond(res){
    return function(result,info){
        var json={
            query_id:info.id,
            pageNum:info.pageNum,
            numOfPages: info.numOfPages,
            data:result
        };
        res.json(json);
    }
}

module.exports = httpapi

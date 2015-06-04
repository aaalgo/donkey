//This file is controller or router for this demo web
//Including api controllers and web page controllers
var fs=require('fs');
var express = require('express');
var config = require('./config');
var router=express.Router();
var donkey =require('./donkey');
var img_cache=require('./image_cache');

var icmgr=new img_cache.ImageCacheManager(config.imageCache.path,config.imageCache.dir,config.imageCache.size);



//api

//get request for search file when img cache is not empty
router.get('/search/file', function(req, res) {
    console.log("file request");
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        res.json({error:"query_id is not valid, please use post."});
    })
});
//post request for search file
router.post('/search/file', function(req, res) {
    req.query=req.body;
    console.log("file request post");
    function sendJson(data){res.json(data);}
	donkey.search(req,sendJson,function(q,callback,info){
		if(req.files.file0==null||req.files.file0.buffer==null||req.files.file0.buffer.length==0){
			res.json({error:"file is not valid"});
		}
		var buf=req.files.file0.buffer;
		try{	
		console.log("before insert");
			icmgr.insert(buf,function(img_id,url){
		console.log("after insert");
				q.url=url;
				info.img_id=img_id;
				callback(null,q);
			})
		}
		catch(err){
		console.log("fail insert");
			callback(err);
		}
	})
});
//get request for url
router.get('/search/url', function(req, res) {
	console.log("url request"+JSON.stringify(req.query));
	function sendJson(data){res.json(data);}
	donkey.search(req,sendJson,function(q,callback){
		if(req.query.url==null){
			res.send({error:"url cannot be empty"});
			callback(1);
		}
		q.url=req.query.url;
		console.log("urlfetch"+q.url);
		callback(null,q);
	})
});


//main page
router.get('/', function(req, res) {
	res.render('index');
});

module.exports = router

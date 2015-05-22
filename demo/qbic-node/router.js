var express = require('express');
var config = require('./config');
var router=express.Router();
var donkey =require('./donkey');



router.get('/search/file', function(req, res) {
    console.log("file request");
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        res.json({error:"query_id is not valid, please use post."});
	return true;
    })
});
router.post('/search/file', function(req, res) {
    console.log("file request post");
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
	if(req.files.file0==null||req.files.file0.buffer==null||req.files.file0.buffer.length==0){
		res.json({error:"file is not valid"});
		return 1;
	}
        q.content=req.files.file0.buffer.toString('ascii');
	return 0;
    })
});
router.get('/search/url', function(req, res) {
    console.log("url request"+JSON.stringify(req.query));
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        q.url=req.query.url;
        if(q.url==null){
            res.send({error:"url cannot be empty"});
		return true;
        }
	return false;
    })
});

router.get('/', function(req, res) {
    res.render('index');
});

module.exports = router

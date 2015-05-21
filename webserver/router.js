var express = require('express');
var config = require('./config');
var router=express.Router();
var donkey =require('./donkey');



router.get('/search/local', function(req, res) {
console.log("local request");
function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        q.content=req.query.content;
    })
});
router.get('/search/url', function(req, res) {
console.log("url request"+JSON.stringify(req.query));
function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        q.url=req.query.url;
	if(q.url==null){
	    res.json({err:"url cannot be empty"});	
	}
    })
});
router.get('/', function(req, res) {
res.render('index');
});

module.exports = router

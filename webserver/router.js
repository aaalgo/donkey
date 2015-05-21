var express = require('express');
var config = require('./config');
var router=express.Router();
var donkey =require('./donkey');



router.get('/search/file', function(req, res) {
    console.log("file request");
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        res.json({error:"query_id is not valid, please use post."});
    })
});
router.post('/search/file', function(req, res) {
    console.log("file request");
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        q.content=req.files.file0.buffer.toString('ascii');
    })
});
router.get('/search/url', function(req, res) {
    console.log("url request"+JSON.stringify(req.query));
    function sendJson(data){res.json(data);}
    donkey.search(req,sendJson,function(q){
        q.url=req.query.url;
        if(q.url==null){
            res.send({error,"error url cannot be empty"});
        }
    })
});

router.get('/', function(req, res) {
    res.render('index');
});

module.exports = router

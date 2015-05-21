var express = require('express');
var authen = require('./authen.js');
var router = express.Router();
var moment      = require('moment');

var defaultPath="/diary/"

var render= function (req,res) {
    name=req.cookies.name || "";
    res.render("login",{pageTitle:"login",name:name});
}
router.get('/', function(req, res) {
    console.log('redirect to default page');
    res.redirect(defaultPath);
});
/* Diary Application */
router.get('/login', function(req, res) {
    if(typeof req.authen=="undefined"){
        render(req,res);
    }else{
        res.redirect(defaultPath);
        
    }
});
router.post('/login', function(req, res) {
    authen.getCode(req.body.name,req.body.passwd,function(code) {
        if(code){
            //login successful
            path=req.cookies.goto || "/";
            res.cookie('user',code,{ expires: moment().add(authen.expire).toDate()});
            res.cookie('name',req.body.name,{ expires: moment().add(moment.duration(1,'years')).toDate()});
            res.redirect(path);
        }else{
            render(req,res);
        }
    });
});
//Page not found handle
router.use(function(req,res) {
    res.status(404).end();
});

module.exports = router;

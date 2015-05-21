/*
 *  Define authen process
 *  
 *  client has cookies.user their side, will be cleared when logout
 *
 *  sessionMap={id:id,timezone:timezone,location:location};
 *  on request:
 *      1. find user in sessionMap2;
 *      2. not find then find it in sessionMap1;
 *      3. if found, means in the same session, then set req.authen, go next;
 *      4. not found search it in database,
 *      5. if in database, new session, put it in sessionMap2, get location, get timezone, set authen
 *      6. not found without authen
 *  login:
 *      1. create new code to store in cookie
 *      2. store this code in database
 *      3. redirect-- set session in next request
 */
var express = require('express');
var http =require('http');
var router = express.Router();
var config = require('../config');

var db       = require('../database/database.js');
var moment      = require('moment');
var crypto=require("crypto");

router.expire=moment.duration(2, 'days');
router.sessionExpire=30000;


//
//cannot contain object
var sessionMap1={};
var sessionMap2={};
setInterval(function (){
    console.log('session expire');
    sessionMap1=sessionMap2;
    sessionMap2={};
    
},router.sessionExpire);

function parseTimezone(timezone) {  //-5.0=>-0500, 5.0=>+0500 for moment().zone()
    timezone='+'+timezone;
    timezone=timezone.replace(/(\d+)\./g,'0$1.');
    return timezone.replace(/.*(\+|-)\d*(\d{2})\.0*/g,'$1$200');
}

/* GET users listing. */
router.use(function(req, res,next) {
    // test code in cookie
    // find it in memory
    var sv=sessionMap2[req.cookies.user];
    if (typeof sv=='undefined') {
        sv=sessionMap1[req.cookies.user];
    }
    if (typeof sv!='undefined') {
        console.log('in session '+'id:'+sv.id+' timezone:'+sv.timezone+' location:'+sv.location);
        sessionMap2[req.cookies.user]=sv;
        req.authen=sv.id; 
        req.timezone=sv.timezone;
        req.location=sv.location;
        next();
    }else{
        //not found in memory, search it in database
        db.connection.query("select id from authen where code=cast(? as binary(128)) and expire> NOW()",
            [req.cookies.user],function(err,rows){
                if (err) {
                    console.error('error connecting: ' + err.stack);
                    res.status(404).end();
                    return;
                }
                // connected! (unless `err` is set)
                if (rows.length){
                    //have logged in
                    console.log('have log in')
                    //new session
                    var sm=sessionMap2[req.cookies.user]={};
                    sm.id=rows[0].id;
                    req.authen=rows[0].id;
                    var originalip=req.headers['x-forwarded-for'];
                    //var originalip='67.78.80.44';
                    getTimezone(originalip,req,function() {
                        getLocation(originalip,req,function () {
                            sm.location=req.location;
                            sm.timezone=req.timezone
                            next();
                        });
                        
                    })
                }else{
                    next();
                }
            })
    }
});

router.getCode= function (name,passwd,next) {
    db.connection.query("select name,id from users where name=cast(? as binary(32)) and passwd=cast(? as binary(128))",
            [name,passwd],function(err,rows){
        if (rows.length){
            //user name matches passwd
            console.log('login')
            var code= crypto.createHash("md5") .update(moment().format('YYDDSS')+rows[0].name).digest("binary");
            db.connection.query("insert into authen set ?",{expire: moment().add(router.expire).toDate(),code:code,id:rows[0].id},function (err,rows){
            });
            next(code);
        } else{
            next(false);
        }
    });
}

function getTimezone(ip,creq,next) {
    option={
        hostname:config.timezonesearch.hostname,
        port:config.timezonesearch.port,
        path:config.timezonesearch.path,
        method:config.timezonesearch.method,
    }
    var data="";
    option.path=option.path+ip;
    var req = http.request(option, function(res) {
        res.setEncoding('utf8');
        res.on('data', function (chunk) {
            data=data+chunk;
        });
        res.on('end',function (){
            var m= JSON.parse(data);
            try {
                var temp=m.search_api.result[0].timezone[0];
                creq.timezone=temp['offset'];
            }
            catch(err) {
                creq.timezone=config.default_timezone;
            }finally{
                creq.timezone=parseTimezone(creq.timezone);
                console.log('timezone:'+creq.timezone);
            }
            next();
        })
    }).on('error', function(e) {
        console.err('problem with request: ' + e.message);
        next();
    }).end();
}

function getLocation(ip,creq,next) {
    option={
        hostname:config.locationsearch.hostname,
        port:config.locationsearch.port,
        path:config.locationsearch.path,
        method:config.locationsearch.method,
    }
    var data="";
    option.path=option.path+ip;
    var req = http.request(option, function(res) {
        res.setEncoding('utf8');
        res.on('data', function (chunk) {
            data=data+chunk;
        });
        res.on('end',function (){
            var m= JSON.parse(data);
                creq.location=m['city']+', '+m['countryCode'];
                if(typeof  m['city']=='undefined'){
                    creq.location='unknown'
                }
            console.log('location:'+creq.location);
            next();
        })
    }).on('error', function(e) {
        console.err('problem with request: ' + e.message);
        next();
    }).end();
}

module.exports = router;

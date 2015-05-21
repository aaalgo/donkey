var express = require('express');
var thrift = require('thrift');
var moment = require('moment');
var config = require('../config');


router.get('/local', function(req, res) {
    res.redirect("/diary/view");
});
router.get('/write', function(req, res) {
    renderWrite(req,res);
});
router.get('/view', function(req, res) {
    renderView(req,res);
});
//diary submit
router.post('/write', function(req, res) {
    doc={
        date:req.cookies.diaryDateUpdated,
        title:req.body.diary_title,
        content:req.body.diary_content,
        author_id:req.authen,
        location:req.location,
    }
    diary.update(doc,function (){});
    res.redirect("/diary/view");
});
//Page not found handle
router.use(function(req,res) {
    res.status(404).end();
});


function renderWrite(req,res) {//

    var date=req.query.d;//date must be YYYYMMDD format
    if (typeof date=='undefined') {
        date=moment().zone(req.timezone).format("YYYY-MM-DD");
    }else{
        lt=(date.length==8);
        date=moment(date,'YYYYMMDD').format("YYYY-MM-DD");
        if(!lt || date=='Invalid date'){
            res.redirect('/diary/write');
            return;
        }
    }

    //find document written on date
    diary.getDiary(req.authen,date,function(doc){
        var dateHuman=moment(date).format("MMM DD");
        var title,content;
        if (typeof doc=='undefined') {
            title=dateHuman;
            content="";
        }else{
            title=doc.title;
            content=doc.content;}
        res.cookie('diaryDateUpdated',date);
        res.render('diary/write', { pageTitle: 'writing' ,date: dateHuman,
            docTitle:title,docContent:content});

    })
}

function renderView(req,res) {
    options={};
    diary.getDiaries(req.authen,options,
        function (rows) {
            var elements='';
            number=rows.length>10?10:rows.length;

            var templateString = null;
            var fs = require('fs');
            var path= require('path');
            fs.readFile(path.join(config.viewsDir,'diary/article.ejs'), 'utf-8', function(err, data) {
                if(!err) {
                    templateString = data.replace(/\r?\n/g,'');
                    for (var i=0;i<number;i++) {
                        elements=elements+ejs.render(templateString,
                            {date: moment(rows[i].date).format("ddd, MMM DD")+',  at '+rows[i].location,
                                title:rows[i].title,
                            content:rows[i].content});
                    }
                    res.render('diary/view', { pageTitle: 'View' ,
                        elements:elements.replace(/\r?\n/g,'<br/>')});
                }
            });
        });
}
module.exports = router;

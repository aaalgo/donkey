var db = require('../database/database');
var moment=require('moment');
var config = require('../config');

var diary={};

/*
 *  doc object
 *  {
 *      id          o
 *      time        o      YYYY-MM-DD HH:mm:SS   UTC
 *      date        io      YYYY-MM-DD           local
 *      title       io
 *      content     io
 *      author_name o
 *      author_id   io
 *      location    io
 *  }
 */


// add author_name<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//options={start: string,end string,num int}
diary.getDiaries=function (author_id,options,func) {
    db.getDiaries(author_id,options,function (rows) {
        var docs=[];
        for (var i=0;i<rows.length;i++) {
            docs[i]={
                id:rows[i].doc_id,
                time:rows[i].doc_date,
                date:rows[i].date,
                title:rows[i].doc_title,
                content:rows[i].doc_content,
                author_id:rows[i].author_id,
                location:rows[i].location
            }
        }
        func(docs)
    });
}

diary.getDiary=function (author_id,date,func) {
    options={start:date,end:date};
    diary.getDiaries(author_id,options, function (rows) {
        func(rows[0]);
    })
}

diary.update=function (doc,func) {
        var item ={doc_date:moment.utc().format('YYYY-MM-DD HH:mm:SS'), 
            date:doc.date,
            doc_title:doc.title,
            doc_content:doc.content,
            location: doc.location,
            author_id:doc.author_id};
    diary.getDiary(doc.author_id,doc.date,function (docToday) {
        if (typeof docToday!='undefined'){
            //already wrote something
            item.doc_id=docToday.id;
            db.updateDiary(item,func);
        } else {
            //have done nothing today
            console.log('add');
            db.addDiary(item,func);
        }
    });
}

module.exports=diary;

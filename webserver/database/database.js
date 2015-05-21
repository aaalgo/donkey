var mysql       = require('mysql');
var config       = require('../config');
var moment= require('moment');
var connection  = mysql.createConnection({
  host     : config.sql_host,
  port     : config.sql_port,
  database : config.sql_database,
  user     : config.sql_user,
  password : config.sql_passwd
});
var db={};
db.connection=connection;

//options{start string,end string,num int,}
db.getDiaries=function (author_id,options,func){
    if(typeof options.end=='undefined'){ options.end=moment().add(2,'year').format('YYYY-MM-DD');
    }
    if(typeof options.start=='undefined'){
        options.start='1997-01-01';
    }
    if(typeof options.num=='undefined'){
        options.num=10000;
    }
    connection.query('select * from document where author_id=? and date>=? and date<=? order by date desc',[author_id,options.start,options.end],
            function (err,rows) {
                if (err) {
                    console.error('error connecting: ' + err.stack);
                    func([]);
                }else{
                // connected! (unless `err` is set)
                if (rows.length>options.num){
                    func(rows.slice(0,options.num));
                }else{
                    func(rows);
                }}
            })
};

db.updateDiary=function (item,func){
    connection.query('update document set ? where doc_id=?',[item,item.doc_id],
            function(err, rows) {
                func();
            });

}
db.addDiary=function (item,func){
    db.connection.query('insert into document set ?',item,
            function(err, rows) {
                func();
            });

}

module.exports = db;

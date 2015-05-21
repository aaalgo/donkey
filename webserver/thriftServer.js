var thrift = require("thrift");
var Donkey = require('./gen-nodejs/Donkey');
var ttypes = require('./gen-nodejs/donkey_types');


var server = thrift.createServer(Donkey, {
    search: 
    function(q, result) {
        var now=new Date();
        var time=now.toTimeString()+" ";
        var data = {};
        if(q.url=="error"){
            var ex={
                what:0,
    why:"intended error"
            };
            result(ex);
        }else{
            for(i=0;i<500;i++){
                data.push(time+i); 
            }
            result({
                time:0.01,
                load_time:0.02,
                filter_time:0.03,
                rank_time:0.04,
                hits:data            

            });
        }
    },

},{});

server.listen(9090);

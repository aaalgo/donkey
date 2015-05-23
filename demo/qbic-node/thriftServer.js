var thrift = require("thrift");
var Donkey = require('./gen-nodejs/Donkey');
var ttypes = require('./gen-nodejs/donkey_types');

var server = thrift.createServer(Donkey, {
    search: 
    function(q, result) {
console.log("@thrift server search: "+JSON.stringify(q));
        var now=new Date();
        var time=now.toTimeString()+" ";
        var data = [];
        if(q.url=="error"){
		
            var ex=new ttypes.Exception();
                ex.what=0;
    ex.why="intended error";
            result(ex);
        }else{
            for(i=0;i<500;i++){
		var hit=new ttypes.Hit();
		hit.key=time+i;
		hit.meta="meta";
		hit.score=i;
                data.push(hit); 
            }
		var res=new ttypes.SearchResponse();
		res.time=0.01;
		res.load_time=0.02;
		res.filter_time=0.03;
		res.rank_time=0.04;
                res.hits=data;           
            result(null,res);
        }
    },

},{});

server.listen(9090);

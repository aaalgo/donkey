function DonkeyQuery(){
    this.params={};
}
DonkeyQuery.prototype={
    searchUrl:function(url,callback){
        this.type="url";
        var querySelf=this;
        if(isNaN(url)){
            this.params.url=url;
            delete this.params.page;
            delete this.params.query_id;
        }
        else{
            querySelf.params.page=url;
        }
        $.get("search/url",this.params,function(data){
            if(data.error==null){
                querySelf.params.query_id=data.query_id;
                querySelf.params.page=data.pageNum;
                querySelf.totalPages=data.numOfPages;
            }
            callback(data);
        });

    },
    gotoPage:function(n,callback){
        if(this.type=="url")
            this.searchUrl(n,callback);
        else this.searchFile(this.file,callback,n);
    },
    searchFile:function searchFile(q,callback,n){
        var querySelf=this;
        var pageInfo=function(data){
            if(data.error==null){
                querySelf.params.query_id=data.query_id;
                querySelf.params.page=data.pageNum;
                querySelf.totalPages=data.numOfPages;
            }
            callback(data);
        }
        function formAppend(form,query){
            for(var i in query){
                form.append(i,query[i]);
            }
        }
        this.type="file";
        this.file=q;
        if(n==null){
            delete this.params.page;
            delete this.params.query_id;
            var form=new FormData();
            form.append("file0",querySelf.file);
            formAppend(form,querySelf.params);
            $.ajax({
                url:"search/file",
                success:pageInfo,
                type:"POST",
                data:form,
                contentType:false,
                processData: false
            })
        }
        else{
            this.params.page=n;
            $.ajax({
                url:"search/file",
                success:function(data){
                    if(data.error!=null){
                        var form=new FormData();
                        form.append("file0",querySelf.file);
                        formAppend(form,querySelf.params);
                        $.ajax({
                            url:"search/file",
                            success:pageInfo,
                            type:"POST",
                            data:form,
                            contentType:false,
                            processData: false
                        })}
                    else pageInfo(data);
                },
                type:"GET",
                data:this.params,
            });
        }
    }
}
qry=new DonkeyQuery();
function next(){
    qry.gotoPage(qry.params.page+1,render);
}
function prev(){
    qry.gotoPage(qry.params.page-1,render);
}

function search(){
    qry.searchUrl($('#url').val(),render);
}
function searchFile(){
    qry.searchFile($('#path')[0].files[0],render);
}


var render=function(data,query){
    if(data.error!=null){
        $("#imgs").html(data.error);
        return;
    }
    var list=data.data.hits;
    var text="";
    var prev='<button type="button" onclick="prev()">prev</td>'
        var next='<button type="button" onclick="next()">next</td>'
        text+="<div>"
        if(data.pageNum>=1)text+=prev;
    if(data.pageNum<data.numOfPages-1)text+=next;
    text+="</div>"
        for(i in list){
            text+='<p>' +  list[i].meta.title +'</p>';
            text+='<img src="'+list[i].meta.thumb+'"></img><br>';
        }
    $("#imgs").html(text);
}

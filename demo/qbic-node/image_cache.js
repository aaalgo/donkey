var fs=require('fs');
var fse=require('fs.extra');
var path=require('path');
var LRU=require('lru-cache');
mod={};


//ImageCacheManager
//cachePath is root path of the cache
//size is the Cache size
//store all images in a subdir
//delete any other directory
var mngr=function(cachePath,cacheDir,size){
	this.dir=cacheDir;
	if(!fs.existsSync(this.dir)){
		console.log("mkdir "+this.dir);
		fs.mkdirSync(this.dir);	
	}
	this.id=(new Date()-0+"");
	this.root=path.join(this.dir,this.id);
	this.url=cachePath+this.id+"/";
	this.cache=LRU({max:size,
		dispose:function(key,value){
			file=path.join(this.root,value);
			fs.unlink(file,function(){});
		}});

	console.log("mkdir "+this.root);
	fs.mkdirSync(this.root);
	var dirs=fs.readdirSync(this.dir);
	for(i in dirs){
		if(this.id!=dirs[i]){
var pa=path.join(this.dir,dirs[i])
			fse.rmrf(pa,function(err){
				if(err)console.error(err);
				else{
					console.log("rmdir "+pa);
				}
			})
		}
	}
}
mod.ImageCacheManager =mngr;

//write buffer to file
//call(img_id,filepath)
mngr.prototype.insert=function(buffer,callback){
	var mngr=this;
	var cur=new Date()-0+"";
	fs.writeFile(path.join(this.root,cur),buffer,function(err){
		if(err)throw err;
		mngr.cache.set(cur,cur);
		callback(cur,mngr.url+cur);
	});
}
mngr.prototype.get=function(id){
	var ans=this.cache.get(id);
	if(ans!=null)
	return this.url+ans;
	return null;
}



module.exports=mod;

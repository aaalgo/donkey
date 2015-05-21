var path= require('path')
var fs= require('fs')
var config={};


if (typeof process.env.OPENSHIFT_NODEJS_PORT!='undefined'){
    //openshift environment
    config.port=process.env.OPENSHIFT_NODEJS_PORT || 8080;
    config.host=process.env.OPENSHIFT_NODEJS_IP;
    config.sql_port=process.env.OPENSHIFT_MYSQL_DB_PORT;
    config.sql_host=process.env.OPENSHIFT_MYSQL_DB_HOST;
    config.sql_database='www';
    config.sql_user='admin2cJ6R8d';
    config.passwdtxt='~/app-root/data/passwd.txt'
} else {
    //local environment
    config.port='80';
    config.host='localhost';
    config.sql_port='3306';
    config.sql_host='localhost';
    config.sql_database='diary';
    config.sql_user='diary';
    config.passwdtxt='./passwd.txt'
}
config.rootDir=__dirname;
config.viewsDir=path.join(config.rootDir,'views');
config.timezonesearch=
{
  hostname: 'api.worldweatheronline.com',
  port: 80,
  path: '/free/v1/search.ashx?key=c1bfbf919dffd79fef61eaa6539a6de75e7df851&num_of_results=1&format=json&timezone=yes&q=',
  method: 'get'
};
config.locationsearch=
{
    hostname: 'ip-api.com',
  port: 80,
  path: '/json/',
  method: 'get'
};
config.default_timezone='-5.0';

var fs = require('fs');
var data=fs.readFileSync(config.passwdtxt, 'utf-8');
passwds=JSON.parse(data);
Object.getOwnPropertyNames(passwds)
    .forEach(function(val, idx, array) {
        config.sql_passwd=passwds[val];
    });

module.exports=config;

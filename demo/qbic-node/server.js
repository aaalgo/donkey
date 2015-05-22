#!/bin/env node
var app = require('./app');
var config = require('./config');

var server = app.listen(config.port,config.host, function() {
  console.log('Express server listening on port ' + server.address().port);
});

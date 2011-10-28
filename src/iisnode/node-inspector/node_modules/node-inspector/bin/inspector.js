#!/usr/bin/env node

var DebugServer = require('../lib/debug-server').DebugServer,
    fs = require('fs'),
    path = require('path'),
    options = {};

process.argv.forEach(function (arg) {
  var parts;
  if (arg.indexOf('--') > -1) {
    parts = arg.split('=');
    if (parts.length > 1) {
      switch (parts[0]) {
      case '--web-port':
        options.webPort = parseInt(parts[1], 10);
        break;
      default:
        console.log('unknown option: ' + parts[0]);
        break;
      }
    }
    else if (parts[0] === '--help') {
      console.log('Usage: node-inspector [options]');
      console.log('Options:');
      console.log('--web-port=[port]     port to host the inspector (default 8080)');
      process.exit();
    }
  }
});

fs.readFile(path.join(__dirname, '../config.json'), function(err, data) {
  var config,
      debugServer;
  if (err) {
    console.warn("could not load config.json\n" + err.toString());
    config = {};
  }
  else {
    config = JSON.parse(data);
    if (config.hidden) {
      config.hidden = config.hidden.map(function(s) {
        return new RegExp(s, 'i');
      });
    }
  }
  // process.env.PORT, if specified, will take precedence over the configured web port
  // NOTE: it need not be TCP port number, it may be a string file descriptor e.g. named pipe name
  config.webPort = process.env.PORT || options.webPort || config.webPort || 8080;
  config.debugPort = process.env.DEBUGPORT || config.debugPort || 5858;
  config.transports = config.transports || ['websocket'];

  debugServer = new DebugServer();
  debugServer.on('close', function () {
    console.log('session closed');
    process.exit();
  });
  debugServer.start(config);
});

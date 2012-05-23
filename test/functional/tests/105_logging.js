/*
Five consecutive requests populate the server side log file that can then be accessed over HTTP
*/

var iisnodeassert = require("iisnodeassert")
    , fs = require('fs')
    , path = require('path')
    , assert = require('assert');

var logDir = path.resolve(__dirname, '../www/105_logging/iisnode');
var existsSync = fs.existsSync || path.existsSync;

if (existsSync(logDir)) {
    fs.readdirSync(logDir).forEach(function (file) {
        fs.unlinkSync(path.resolve(logDir, file));
    });
}

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/105_logging/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 3"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 4"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 5"),
    function (next) {
        assert.ok(existsSync(logDir), 'log directory exists');
        var files = fs.readdirSync(logDir)
        assert.ok(files.length === 2, 'two files created in the log directory');
        var index;
        var log;
        files.forEach(function (file) {
            if (file === 'index.html')
                index = true;
            else
                log = fs.readFileSync(path.resolve(logDir, file), 'utf8');
        });

        assert.ok(index, 'index.html file created');
        assert.equal(log, "Request1\r\nRequest2\r\nRequest3\r\nRequest4\r\nRequest5\r\n", 'all log entries are present');

        if (next)
            next();
    }
]);
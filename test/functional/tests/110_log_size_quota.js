/*
Another log file is created if log size quota is reached
*/

var iisnodeassert = require("iisnodeassert")
    , fs = require('fs')
    , path = require('path')
    , assert = require('assert');

var logDir = path.resolve(__dirname, '../www/110_log_size_quota/iisnode');
var existsSync = fs.existsSync || path.existsSync;

if (existsSync(logDir)) {
    fs.readdirSync(logDir).forEach(function (file) {
        fs.unlinkSync(path.resolve(logDir, file));
    });
}

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/110_log_size_quota/hello.js", 200, "Hello, world"),
    iisnodeassert.get(2000, "/110_log_size_quota/hello.js", 200, "Hello, world"),
    function (next) {
        assert.ok(existsSync(logDir), 'log directory exists');
        var files = fs.readdirSync(logDir)
        assert.equal(files.length, 3, 'three files created in the log directory');

        if (next)
            next();
    }
]);
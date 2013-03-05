/*
A 31MB request with 30MB response
*/

var assert = require('assert');

var timeout = setTimeout(function () {
    console.error('Timeout occurred');
    assert.ok(false, 'request timed out');
}, 10000);

var host = process.env.IISNODETEST_HOST || 'localhost';
var port = process.env.IISNODETEST_PORT || 31415;

var options = {
    hostname: host,
    port: port,
    path: '/140_large_download/hello.js',
    method: 'POST'
};

var req = require('http').request(options, function (res) {
    assert.equal(res.statusCode, 200);
    res.setEncoding('binary');
    var contentLength = 0;
    res.on('data', function (data) {
        contentLength += data.length;
    });
    res.on('end', function () {
        clearTimeout(timeout);
        assert.equal(contentLength, 30 * 1024 * 1024);
    });
});

req.on('error', function (e) {
    assert.ifError(e);
});

var buffer = new Buffer(1024);
for (var i = 0; i < (31 * 1024) ; i++)
    req.write(buffer);
req.end();

/*
A 31MB upload
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
    path: '/141_large_upload/hello.js',
    method: 'POST'
};

var req = require('http').request(options, function (res) {
    assert.equal(res.statusCode, 200);
    res.setEncoding('utf8');
    var body = ''
    res.on('data', function (chunk) {
        body += chunk;
    });
    res.on('end', function () {
        clearTimeout(timeout);
        assert.equal(body, 31 * 1024 * 1024);
    });
});

req.on('error', function (e) {
    assert.ifError(e);
});

var buffer = new Buffer(1024);
for (var i = 0; i < (31 * 1024) ; i++)
    req.write(buffer);
req.end();

/*
A 30MB download
*/

var assert = require('assert');

var timeout = setTimeout(function () {
    console.error('Timeout occurred');
    assert.ok(false, 'request timed out');
}, 10000);

var host = process.env.IISNODETEST_HOST || 'localhost';
var port = process.env.IISNODETEST_PORT || 31415;

require('http').get('http://' + host + ':' + port + '/140_large_download/hello.js', function (res) {
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
}).on('error', function (e) {
    assert.ifError(e);
});

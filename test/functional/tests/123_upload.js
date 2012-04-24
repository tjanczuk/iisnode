/*
Uploading data with and without chunked transfer encoding works
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.post(10000, "/123_upload/hello.js", { body: 'abc', chunked: true }, 200, 'true-abc'),
    iisnodeassert.post(2000, "/123_upload/hello.js", { body: 'def' }, 200, 'false-def'),
    iisnodeassert.post(2000, "/123_upload/hello.js", { body: '', chunked: true, headers: { 'transfer-encoding': 'chunked'} }, 200, 'true-'),
    iisnodeassert.post(2000, "/123_upload/hello.js", { body: '' }, 200, 'false-'),
    function (next) {
        // test for multi-chunk upload

        var net = require('net')
            , assert = require('assert');

        var timeout = setTimeout(function () {
            console.error('Timeout occurred');
            assert.ok(false, 'request timed out');
            next();
        }, 2000);

        var host = process.env.IISNODETEST_HOST || 'localhost';
        var port = process.env.IISNODETEST_PORT || 31415;

        var client = net.connect(port, host, function () {
            client.setEncoding('utf8');
            client.write('POST /123_upload/hello.js HTTP/1.1\r\nHost: ' + host + ':' + port + '\r\nTransfer-Encoding: chunked\r\n\r\n'
                + '3\r\nabc\r\n4\r\ndefg\r\n0\r\n\r\n');
        });

        client.on('data', function (data) {
            clearTimeout(timeout);
            console.log('Received response: ' + data);
            assert.ok(data.indexOf('true-abcdefg') > 0, 'Request contains two chunks of data in the entity body');
            client.end();
            next();
        });
    }
]);
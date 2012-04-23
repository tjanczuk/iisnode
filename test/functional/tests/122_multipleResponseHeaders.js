/*
A simple GET request receives a hello world response
*/

var net = require('net')
    , assert = require('assert');

var timeout = setTimeout(function () {
    console.error('Timeout occurred');
    assert.ok(false, 'request timed out');
}, 10000);

var host = process.env.IISNODETEST_HOST || 'localhost';
var port = process.env.IISNODETEST_PORT || 31415;

var client = net.connect(port, host, function () {
    client.setEncoding('utf8');
    client.write('GET /122_multipleResponseHeaders/hello.js HTTP/1.1\r\nHost: ' + host + ':' + port + '\r\n\r\n');
});

client.on('data', function (data) {
    clearTimeout(timeout);
    assert.ok(data.indexOf('Foo: Val1') > 0, 'First instance of Foo header exists in the response');
    assert.ok(data.indexOf('Foo: Val2') > 0, 'Second instance of Foo header exists in the response');
    client.end();
});
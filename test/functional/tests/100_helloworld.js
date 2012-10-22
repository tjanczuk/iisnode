/*
A simple GET request receives a hello world response
*/

var iisnodeassert = require("iisnodeassert")
    , assert = require('assert');

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/100_helloworld/hello.js", 200, "Hello, world!", function (res) {
        assert.ok(typeof res.headers['iisnode-debug'] === 'undefined', 'iisnode-debug is absent');
    })
]);
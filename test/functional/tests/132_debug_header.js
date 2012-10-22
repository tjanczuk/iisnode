/*
A response contains the iisnode-debug header
*/

var iisnodeassert = require("iisnodeassert")
    , assert = require('assert');

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/132_debug_header/hello.js", 200, "Hello, world!", function (res) {
        assert.ok(typeof res.headers['iisnode-debug'] === 'string', 'header is present');
        assert.ok(res.headers['iisnode-debug'].indexOf('http://bit.ly/NsU2nd#') === 0, 'header value is a URL');
    })
]);
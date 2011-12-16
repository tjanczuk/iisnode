/*
Fixed length and chunked transfer encoding responses are received.
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/113_encoding/hello.js", 200, "content-length response"),
    iisnodeassert.get(2000, "/113_encoding/hello.js?onechunk", 200, "chunked response"),
    iisnodeassert.get(4000, "/113_encoding/hello.js?tenchunks", 200, "0123456789")
]);
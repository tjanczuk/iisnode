/*
A simple GET request receives a hello world response
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/100_helloworld/hello.js", 200, "Hello, world!")
]);
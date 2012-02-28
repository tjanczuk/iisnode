/*
A simple GET request receives a hello world response despite watched files directories do not exist
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/121_watchedFiles/hello.js", 200, "Hello, world!")
]);
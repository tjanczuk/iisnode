/*
Accessing hello world service after the the *.js file has been updated
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/117_autoupdate_dependency/hello.js", 200, "Hello, world 2")
]);
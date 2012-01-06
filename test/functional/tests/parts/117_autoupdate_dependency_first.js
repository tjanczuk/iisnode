/*
Accessing hello world service before the dependent *.js file is updated
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/117_autoupdate_dependency/hello.js", 200, "Hello, world 1")
]);
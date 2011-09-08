/*
Accessing hello world service before the *.js file is updated
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/106_autoupdate/hello.js", 200, "Hello, world 1")
]);
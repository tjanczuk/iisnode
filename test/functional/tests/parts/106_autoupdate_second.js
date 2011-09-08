/*
Accessing hello world service after the *.js file has been updated
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/106_autoupdate/hello.js", 200, "Hello, world 2")
]);
/*
Current directory of node.exe process is set to the same location as the applications's *.js file.
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/107_filesystem/hello.js", 200, "Contents of a file")
]);
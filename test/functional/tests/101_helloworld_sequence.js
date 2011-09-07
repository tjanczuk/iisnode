/*
Five consecutive requests are dispatched to the same node.exe process
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/101_helloworld_sequence/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/101_helloworld_sequence/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/101_helloworld_sequence/hello.js", 200, "Hello, world 3"),
    iisnodeassert.get(2000, "/101_helloworld_sequence/hello.js", 200, "Hello, world 4"),
    iisnodeassert.get(2000, "/101_helloworld_sequence/hello.js", 200, "Hello, world 5")
]);
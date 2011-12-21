/*
Four consecutive requests are dispatched to two node.exe processes following a round robin algorithm
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/114_roundrobin/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/114_roundrobin/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/114_roundrobin/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/114_roundrobin/hello.js", 200, "Hello, world 2")
]);
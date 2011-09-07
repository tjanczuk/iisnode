/*
Five consecutive requests populate the server side log file that can then be accessed over HTTP
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/105_logging/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 3"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 4"),
    iisnodeassert.get(2000, "/105_logging/hello.js", 200, "Hello, world 5"),
    iisnodeassert.wait(1000), // wait for 1 second - configured log flush interval is 500 ms
    iisnodeassert.get(2000, "/105_logging/hello.js.logs/0.txt", 200, "Request1\nRequest2\nRequest3\nRequest4\nRequest5\n")
]);
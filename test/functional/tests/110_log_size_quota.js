/*
Log file is truncated if it exceeds configured maximum size
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/110_log_size_quota/hello.js", 200, "Hello, world"),
    iisnodeassert.wait(1000), // wait for 1 second - configured log flush interval is 500 ms
    iisnodeassert.get(2000, "/110_log_size_quota/hello.js.logs/0.txt", 200, ">>>> iisnode truncated the log file because it exceeded the configured maximum size\n")
]);
/*
When devErrorsEnabled="true", iis failures are reported as HTTP 200, otherwise 5xx
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/112_dev_errors/on/hello.js", 200),
    iisnodeassert.get(10000, "/112_dev_errors/off/hello.js", 500)
]);
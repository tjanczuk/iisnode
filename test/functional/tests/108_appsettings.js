/*
appSettings from web.config are propagated to the node.exe process as environment variables
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/108_appsettings/hello.js", 200, "value1value2")
]);
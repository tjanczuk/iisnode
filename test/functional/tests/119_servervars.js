/*
Server variables specified in the iisnode\@promoteServerVars configuration section are promoted to X-iisnode-* headers
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/119_servervars/hello.js", 200, "x-iisnode-request_method: GET")
]);
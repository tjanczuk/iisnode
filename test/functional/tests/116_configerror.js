/*
Local request to iisnode with a configuration syntax error returns a friendly 200 response
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/116_configerror/hello.js", 200)
]);
/*
iisnode adds X-Forwarded-For header to HTTP requests when issnode\@enableXFF is true
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/118_xff/hello.js", 200, "Request contains X-Forwarded-For header")
]);
/*
iisnode adds X-Forwarded-For header to HTTP requests when issnode\@enableXFF is true
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/118_xff/hello.js", 200, "Request contains X-Forwarded-For and X-Forwarded-Proto headers", { 'x-echo-x-forwarded-for': '127.0.0.1', 'x-echo-x-forwarded-proto': 'http' }),
    iisnodeassert.post(10000, "/118_xff/hello.js", { headers: { 'X-Forwarded-Proto': 'https' }}, 200, "Request contains X-Forwarded-For and X-Forwarded-Proto headers", { 'x-echo-x-forwarded-for': '127.0.0.1', 'x-echo-x-forwarded-proto': 'https' })
]);
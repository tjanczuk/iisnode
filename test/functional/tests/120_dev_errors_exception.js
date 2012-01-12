/*
When devErrorsEnabled="true", iisnode returns a friendly HTTP 200 response if unhandled exceptions are thrown in the application
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/120_dev_errors_exception/hello.js", 200)
]);
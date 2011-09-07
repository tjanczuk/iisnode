/*
A simple POST request receives an echo of the request body
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.post(10000, "/104_echo/echo.js", "A simple body", 200, "A simple body")
]);
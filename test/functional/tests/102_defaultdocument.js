/*
A request URL without the *.js file specified picks up the index.js registered as the default document in web.config
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/102_defaultdocument/", 200, "[defaultdocument]")
]);

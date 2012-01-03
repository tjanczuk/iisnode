/*
Custom request and response headers are processed correctly
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.post(10000, "/115_customheaders/hello.js", { headers: { 'x-foo': 'foobar'} }, 200, "Hello, world", { 'x-bar': 'foobar', 'x-got-foo': 'true' }),
    iisnodeassert.post(2000, "/115_customheaders/hello.js", { headers: { 'x-foo': ''} }, 200, "Hello, world", { 'x-bar': '', 'x-got-foo': 'true' }),
    iisnodeassert.post(2000, "/115_customheaders/hello.js", { headers: { 'x-foo': '', 'x-baz': 'foobar' } }, 200, "Hello, world", { 'x-bar': '', 'x-got-foo': 'true' })
]);
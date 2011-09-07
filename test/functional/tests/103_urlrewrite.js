/*
Request to URLs subordinate of 'hello' get handled by the hello.js application
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/103_urlrewrite/hello", 200, "/103_urlrewrite/hello"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello/", 200, "/103_urlrewrite/hello/"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello.js", 200, "/103_urlrewrite/hello.js"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello?foo=bar", 200, "/103_urlrewrite/hello?foo=bar"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello/a/b/c", 200, "/103_urlrewrite/hello/a/b/c"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello/a/b/c/", 200, "/103_urlrewrite/hello/a/b/c/"),
    iisnodeassert.get(2000, "/103_urlrewrite/hello/a/b?foo=bar", 200, "/103_urlrewrite/hello/a/b?foo=bar")
]);
/*
Testing samples
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/node/helloworld/hello.js", 200),
    iisnodeassert.get(10000, "/node/configuration/hello.js", 200, "Hello, world! [configuration sample]"),
    iisnodeassert.get(10000, "/node/logging/hello.js", 200, "Hello, world! [logging sample]"),
    iisnodeassert.get(10000, "/node/defaultdocument/", 200, "You have reached the default node.js application at index.js! [defaultdocument sample]"),
    iisnodeassert.get(10000, "/node/express/myapp/foo", 200, "Hello from foo! [express sample]"),
    iisnodeassert.get(2000, "/node/express/myapp/bar", 200, "Hello from bar! [express sample]"),
    iisnodeassert.get(2000, "/node/express/myapp/idonotexist", 404, "Cannot GET /node/express/myapp/idonotexist"),
    iisnodeassert.get(10000, "/node/urlrewrite/myapp/foo/bar/baz", 200, "Hello from urlrewrite sample. Request path: /node/urlrewrite/myapp/foo/bar/baz"),
    iisnodeassert.get(2000, "/node/urlrewrite/myapp/1/2/3?param=4", 200, "Hello from urlrewrite sample. Request path: /node/urlrewrite/myapp/1/2/3?param=4")
]);
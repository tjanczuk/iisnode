/*
In production environment debugging and logging are disabled and NODE_ENV is set to 'production'
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/111_node_env/hello.js", 200, "Hello, world! [NODE_ENV]=production"),
    iisnodeassert.get(2000, "/111_node_env/hello.js/debug", 200, "Hello, world! [NODE_ENV]=production"),
    iisnodeassert.get(2000, "/111_node_env/hello.js.logs/0.txt", 404)
]);
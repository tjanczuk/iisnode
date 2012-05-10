/*
Number of processes has been changed from 2 specified in web.config to 4 specified in iisnode.yml
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/124_node_config_override/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 1"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 2"),
    iisnodeassert.get(2000, "/124_node_config_override/hello.js", 200, "Hello, world 2")
]);
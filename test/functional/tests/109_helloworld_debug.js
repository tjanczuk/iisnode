/*
A simple GET request receives a hello world response
*/

var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug", 301),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/", 200),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/?foo", 200, "Unrecognized debugging command. Supported commands are ?debug (default), ?brk, and ?kill."),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/?kill", 200, "The debugger and debugee processes have been killed."),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/?kill", 200, "The debugger for the application is not running."),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/?brk", 200),
    iisnodeassert.get(10000, "/100_helloworld/hello.js/debug/?kill", 200, "The debugger and debugee processes have been killed.")
]);
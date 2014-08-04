/*
A simple test to see if recycle signal from node app worked.
*/

var iisnodeassert = require("iisnodeassert")
    , assert = require('assert');
    
var firstPid = 0;
var secondPid = 0;

iisnodeassert.sequence([
iisnodeassert.get(10000, "/142_recycle_signal/hello.js", 200, "Hello, world!", function (res) {
        firstPid = res.headers['processpid'];
        console.log(firstPid);      
    })
]);

setTimeout(function() {
iisnodeassert.sequence([
iisnodeassert.get(10000, "/142_recycle_signal/hello.js", 200, "Hello, world!", function (res) {
        secondPid = res.headers['processpid'];
        console.log(secondPid);     
        assert.ok( firstPid != secondPid, 'recycle was not successful'); 
    })
]);
}, 10000);
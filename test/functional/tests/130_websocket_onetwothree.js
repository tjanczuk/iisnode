/*
A WebSocket connection receives 3 messages from the server
*/

var WebSocket = require('faye-websocket')
    , iisnodeassert = require('iisnodeassert')
    , assert = require('assert');

var lines = [];
var address = iisnodeassert.getAddress('ws', '/130_websocket_onetwothree/server.js');
var ws = new WebSocket.Client(address);

var timeout = setTimeout(function () {
    assert.fail('10s', '< 10s', 'The messages had not been received within 10s');
}, 10000);

ws.onmessage = function (event) {
    lines.push(event.data);
};

ws.onclose = function (event) {
    clearTimeout(timeout);
    assert.strictEqual(lines[0], 'one');
    assert.strictEqual(lines[1], 'two');
    assert.strictEqual(lines[2], 'three');
};

ws.onerror = function (error) {
    assert.ifError(error);
};

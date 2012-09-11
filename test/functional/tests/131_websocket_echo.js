/*
A WebSocket connection receives 3 messages from the server
*/

var WebSocket = require('faye-websocket')
    , iisnodeassert = require('iisnodeassert')
    , assert = require('assert');

var lines = ['The', 'cat', 'in', 'the', 'hat'];
var address = iisnodeassert.getAddress('ws', '/131_websocket_echo/server.js');
//var address = 'ws://localhost:8888/';
var ws = new WebSocket.Client(address);
var receiveIndex = 0;

var timeout = setTimeout(function () {
    assert.fail('10s', '< 10s', 'The messages had not been echoed within 10s');
}, 10000);

ws.onmessage = function (event) {
    console.log('onmessage: ' + event.data);
    assert.strictEqual(lines[receiveIndex], event.data);
    receiveIndex++;
    if (receiveIndex === lines.length)
        ws.close();
};

ws.onclose = function (event) {
    console.log('onclose');
    clearTimeout(timeout);
    assert.strictEqual(lines.length, receiveIndex, 'All messages were received');
};

ws.onerror = function (error) {
    console.log('onerror');
    assert.ifError(error);
};

ws.onopen = function () {
    console.log('onopen');
    var scheduleSendLine = function (line) {
        if (line < lines.length)
            setTimeout(function () {
                console.log('sending ' + lines[line]);
                ws.send(lines[line]);
                scheduleSendLine(line + 1);
            }, 200);
    };

    console.log('sending ' + lines[0]);
    ws.send(lines[0]);
    scheduleSendLine(1);
};
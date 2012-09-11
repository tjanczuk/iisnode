
var WebSocket = require('faye-websocket')
    , http = require('http');

var server = http.createServer(function (req, res) {
    res.writeHead(400);
    res.end();
});

server.addListener('upgrade', function (request, socket, head) {
    var ws = new WebSocket(request, socket, head);

    ws.onmessage = function (event) {
        if (ws)
            ws.send(event.data);
    };

    ws.onclose = function (event) {
        ws = null;
    };
});

server.listen(process.env.PORT || 8888);

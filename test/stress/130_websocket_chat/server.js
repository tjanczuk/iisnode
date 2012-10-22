
var WebSocket = require('faye-websocket')
    , http = require('http');

var server = http.createServer(function (req, res) {
    res.writeHead(400);
    res.end();
});

var clients = {};
var clientId = 0;

server.addListener('upgrade', function (request, socket, head) {
    var ws = new WebSocket(request, socket, head);
    ws.clientId = clientId++;
    clients[ws.clientId] = ws;

    ws.onmessage = function (event) {
        var deadClients = [];
        for (var id in clients) {
            var ws = clients[id];
            try {
                ws.send(JSON.stringify({ 
                    message: event.data,
                    pid: process.pid,
                    memory: process.memoryUsage() 
                }));
            }
            catch (e) {
                deadClients.push(id);
            }
        }

        deadClients.forEach(function (id) {
            delete clients[id];
        });
    };

    ws.onclose = function (event) {
        delete clients[ws.clientId];
    };
});

server.listen(process.env.PORT || 8888);


var WebSocket = require('faye-websocket')
    , http = require('http');

var lines = ['one', 'two', 'three'];

var server = http.createServer(function (req, res) {
    res.writeHead(400);
    res.end();
});

server.addListener('upgrade', function (request, socket, head) {
    console.log(request.headers);
    var ws = new WebSocket(request, socket, head);

    function schedule(line) {
        if (line < lines.length)
            setTimeout(function () {
                if (ws) {
                    ws.send(lines[line]);
                    schedule(line + 1);
                }
            }, 300);
        else if (ws)
            ws.close();
    }

    ws.onclose = function (event) {
        ws = null;
    };

    ws.send(lines[0]);
    schedule(1);
});

server.listen(process.env.PORT || 8888);

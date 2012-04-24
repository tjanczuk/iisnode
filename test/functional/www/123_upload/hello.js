var http = require('http');

http.createServer(function (req, res) {
    var response = req.headers['transfer-encoding'] === 'chunked' ? 'true-' : 'false-';

    req.on('data', function (chunk) {
        response += chunk;
    });

    req.on('end', function () {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(response);
    });
}).listen(process.env.PORT);  
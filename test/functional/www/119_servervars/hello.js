var http = require('http');

http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('x-iisnode-request_method: ' + req.headers['x-iisnode-request_method']);
}).listen(process.env.PORT);  
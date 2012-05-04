var http = require('http');

var n = 1;

http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('Hello, world ' + n++);
}).listen(process.env.PORT);  
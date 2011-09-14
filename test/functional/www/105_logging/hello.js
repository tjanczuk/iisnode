var http = require('http');

var n = 1;

http.createServer(function (req, res) {
    console.log('Request' + n);
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('Hello, world ' + n++);
}).listen(process.env.PORT);  
var http = require('http');

var s = "01";
for (var i = 0; i < 9; i++) s = s + s;

http.createServer(function (req, res) {
    console.log(s);
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('Hello, world');
}).listen(process.env.PORT);  
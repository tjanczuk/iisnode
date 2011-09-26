var http = require('http');
var fs = require('fs');

http.createServer(function (req, res) {
    fs.readFile('file.txt', function (e, d) {
        if (e) throw e;
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(d);
    });
}).listen(process.env.PORT);  
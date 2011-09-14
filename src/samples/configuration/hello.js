var http = require('http');

http.createServer(function (req, res) {
    console.log('A new request arrived with HTTP headers: ' + JSON.stringify(req.headers));
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('Hello, world! [configuration sample]');
}).listen(process.env.PORT);

console.log('Application started at location ' + process.env.PORT);
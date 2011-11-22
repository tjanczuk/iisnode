var http = require('http');

http.createServer(function (req, res) {
    console.log('logging to /dev/null');
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('Hello, world! [NODE_ENV]=' + process.env.NODE_ENV);
}).listen(process.env.PORT);  
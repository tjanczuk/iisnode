var http = require('http');

http.createServer(function (req, res) {
    console.log(JSON.stringify(req.headers));
    res.setHeader('Content-Type', 'text/html');
    if (req.headers.hasOwnProperty('x-foo')) {
        res.setHeader('x-bar', req.headers['x-foo']);
        res.setHeader('x-got-foo', 'true');
    }
    res.writeHead(200);
    res.end('Hello, world');
}).listen(process.env.PORT);  
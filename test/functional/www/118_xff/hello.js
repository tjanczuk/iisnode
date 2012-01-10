var http = require('http');

http.createServer(function (req, res) {
    res.setHeader('Content-Type', 'text/html');
    if (req.headers.hasOwnProperty('x-forwarded-for')) {
        res.setHeader('x-echo-x-forwarded-for', req.headers['x-forwarded-for']);
        res.writeHead(200);
        res.end('Request contains X-Forwarded-For header');
    }
    else {
        res.writeHead(200);
        res.end('Request does not contain X-Forwarded-For header');
    }
}).listen(process.env.PORT);  
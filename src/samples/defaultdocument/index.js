var http = require('http');

http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('You have reached the default node.js application at index.js! [defaultdocument sample]');
}).listen(process.env.PORT);  
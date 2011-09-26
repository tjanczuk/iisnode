var http = require('http');

http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('' + process.env.setting1 + process.env.setting2);
}).listen(process.env.PORT);  
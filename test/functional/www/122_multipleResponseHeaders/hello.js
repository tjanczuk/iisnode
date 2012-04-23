var http = require('http');

http.createServer(function (req, res) {
    res.writeHead(200, [['Foo','Val1'], ['Foo','Val2']]);
    res.end('Hello, world!');
}).listen(process.env.PORT);  
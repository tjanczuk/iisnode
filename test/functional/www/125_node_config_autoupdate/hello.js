var http = require('http');

var n = 1;

http.createServer(function (req, res) {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    var query = require('url').parse(req.url).query;
    if (query === 'update') {
        require('fs').writeFileSync(require('path').resolve(__dirname, 'iisnode.yml'), 'nodeProcessCountPerApplication: 4');
        res.end('iisnode.yml updated');
    }
    else if (query === 'revert') {
        require('fs').writeFileSync(require('path').resolve(__dirname, 'iisnode.yml'), 'nodeProcessCountPerApplication: 2');
        res.end('iisnode.yml reverted');
    }
    else {
        res.end('Hello, world ' + n++);
    }
}).listen(process.env.PORT);  
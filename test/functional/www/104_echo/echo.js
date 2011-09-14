var http = require('http');

http.createServer(function (req, res) {
    var body = "";
    console.log('Request received');
    req.on("data", function (chunk) {
        console.log('Body chunk: ' + chunk);
        body += chunk;
    });
    req.on("end", function () {
        console.log('End of body');
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(body);
    });
}).listen(process.env.PORT);  
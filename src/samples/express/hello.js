var express = require('express');

var app = express.createServer();

app.get('/node/express/myapp/foo', function (req, res) {
    res.send('Hello from foo! [express sample]');
});

app.get('/node/express/myapp/bar', function (req, res) {
    res.send('Hello from bar! [express sample]');
});

app.listen(process.env.PORT);
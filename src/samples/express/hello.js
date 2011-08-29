var express = require('express');

var app = express.createServer();

app.get('/node/express/hello.js', function (req, res) {
    res.send('Hello, World! [express sample]');
});

app.listen(process.env.PORT);
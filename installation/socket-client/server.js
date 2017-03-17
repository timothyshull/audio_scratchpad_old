/*jslint node: true, nomen: true*/
'use strict';
var spawn = require('child_process').spawn,
    http = require('http'),
    _ = require('lodash'),
    server,
    handler,
    io,
    temperPoll;

handler = function handler(req, res) {
    /*jslint unparam: true*/
    console.log('client connected');
    res.writeHead(200);
};

server = http.createServer(handler);
io = require('socket.io')(server);

server.listen(7005, function () {
    console.log("Server listening");
});

server.on('error', function () {
    process.exit();
});

temperPoll = function temperPoll(sock) {
    var temp = spawn('temper-poll'),
        re = /°C\s(.*)°F/,
        str,
        val;

    temp.stdout.on('data', function (data) {
        str = data.toString();
        val = str.match(re);

        if (_.isArray(val) && val[1]) {
            console.dir(val[1]);
            sock.emit('temp.res', val[1]);
        }

    });

    temp.stderr.on('data', function (data) {
        console.log('stderr: ' + data);
    });

    temp.on('close', function (code) {
        console.log('child process exited with code ' + code);
    });
};


io.on('connection', function (socket) {
    socket.on('temp.poll', function () {
        setInterval(function () {
            temperPoll(socket);
        }, 2000);
    });
});
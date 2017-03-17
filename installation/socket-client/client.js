/*jslint node: true, nomen: true*/
'use strict';
var socket = require('socket.io-client')('http://192.168.2.2:7005'),
    _ = require('lodash'),
    osc = require('osc'),
    udpPort = new osc.UDPPort({
        localAddress: '0.0.0.0',
        localPort: 7004
    });


udpPort.on("message", function (oscMsg) {
    console.dir(oscMsg);
    var event = _.get(oscMsg, 'address');
    console.log(event);
    if (event === '/connect') {
        socket.emit('temp.poll');
    }
});

// Open the socket.
udpPort.open();

console.log('upd port opened');

socket.on('connect', function () {
    console.log('socket connected');
});

socket.on('temp.res', function (data) {
    var str = data.toString(),
        temp;

    console.log(str);

    try {
        temp = parseFloat(str);
        udpPort.send({
            address: '/temp.res',
            args: [temp]
        }, '127.0.0.1', 7006);
    } catch (e) {
        console.dir(e);
    }
});

socket.on('disconnect', function () {
    process.exit();
});
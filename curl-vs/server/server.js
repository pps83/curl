'use strict';

const WebSocket = require('ws')
const wss = new WebSocket.Server({ port: 1122 })
wss.on('connection', ws => {
    console.log(' *** opened');
    ws.on('close', () => { console.log(' *** closed'); });
    ws.send('Hello!')
})

#!/usr/bin/env node
const yargs = require('yargs/yargs');
const { hideBin } = require('yargs/helpers');
const argv = yargs(hideBin(process.argv)).argv;

const { SerialPort } = require('serialport');

const config = {
    periodMs: 1,
    numPackets: 2,
    portPath: '/dev/tty.usbserial-AH03FH1E',
    baudRate: 230400,
};

// At 230400 baud, around 23040 bytes/sec theoretically

// With periodMs = 4 and numPackets = 1, everything works perfectly. It's 2000 bytes/sec theoretically though it 
// seems a little lower than that from the count on the device side

// With periodMs = 3 and numPackets = 1, I lose significant bytes, which seems a little low. It's 2667 bytes/sec
// After increasing the Serial1 buffer size ot 512 bytes, this now works.

// With periodMs = 2 and numPackets = 1, no data loss with 512 byte serial buffer. Theoretically 4000 bytes/sec.
// After increasing the Serial1 buffer size ot 512 bytes, this now works.

// With periodMs = 1 and numPackets = 1, no data loss with 512 byte serial buffer. Theoretically 8000 bytes/sec.
// After increasing the Serial1 buffer size ot 512 bytes, this now works.

// With periodMs = 1 and numPackets = 2, no data loss with 512 byte serial buffer. Theoretically 16000 bytes/sec.
// With 6.3.0, once I start getting errors they cascade, probably due to slow USB serial logging
// With develop (2025-03-12) I get occasional errors, but only one at a time instead of continuous
// 0000140639 [app] INFO: good=197573 bad=31 pct=99 kbytesRcvd=1580.0 13987 bytes/sec


let lastSequence = 0;

const port = new SerialPort({
    path: config.portPath,
    baudRate: config.baudRate,
});

port.on('data', function (data) {
    console.log('Data:', data)
});

async function sendPacket() {
    await new Promise(function(resolve, reject) {      

        const buf = Buffer.alloc(8);

        buf.write('****', 0);
        buf.writeUInt32LE(++lastSequence, 4);
    
        port.write(buf, function(err) {
            if (err) {
                console.log('serial write error', err.message);
                reject(err.message);
            }        
            resolve();
        });    
    });
}

setInterval(async function() {
    for(let ii = 0; ii < config.numPackets; ii++) {
        await sendPacket();
    }    
}, config.periodMs);

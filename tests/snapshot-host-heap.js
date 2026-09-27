'use strict';
const ivm = require('isolated-vm');

// Used to abort with `Check failed: (backing_store) != nullptr` once the caller's heap was over memoryLimit
const snapshot = ivm.Isolate.createSnapshot([ { code: 'globalThis.buffer = new Uint8Array(1024);' } ]);
const memoryLimit = 8;
const hold = [];
while (process.memoryUsage().heapUsed < (memoryLimit + 16) * 1024 * 1024) {
	hold.push(new Array(1024).fill({}));
}
new ivm.Isolate({ memoryLimit, snapshot }).dispose();
hold.length = 0;
console.log('pass');

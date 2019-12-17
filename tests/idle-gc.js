const ivm = require('isolated-vm');
const isolate = new ivm.Isolate({ memoryLimit: 128 });
const context = isolate.createContextSync();
const jail = context.global;
setTimeout(() => console.log('pass'), 15000);

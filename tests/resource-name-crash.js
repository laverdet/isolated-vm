const { Isolate } = require('isolated-vm');
const isolate = new Isolate({ memoryLimit: 8 });
const ctx = isolate.createContextSync();
ctx.eval('~'.repeat(1e6)).catch(() => console.log('pass'));

'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let size1 = isolate.getHeapStatisticsSync();
let context = isolate.createContextSync();
let global = context.global;
global.setSync('global', global.derefInto());
let script = isolate.compileScriptSync(`
global.data = new Uint8Array(1024);
global.garbage = [];
for (let ii = 0; ii < 100000; ++ii) {
	garbage.push(Math.random());
}
`);
script.runSync(context);
let size2 = isolate.getHeapStatisticsSync();
if (size1.total_heap_size === size2.total_heap_size) {
	console.log('heap didn\'t increase?');
} else if (size2.externally_allocated_size !== 1024) {
	console.log('externally allocated size is wrong');
} else {
	console.log('pass');
}

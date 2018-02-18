'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 32 });
for (let ii = 0; ii < 500; ++ii) {
	let context = isolate.createContextSync();
	context.release();
}
console.log('pass');

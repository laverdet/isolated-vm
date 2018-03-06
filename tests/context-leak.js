'use strict';
let ivm = require('isolated-vm');
let snapshot = ivm.Isolate.createSnapshot([
	{ code: "function fn(){};" }
]);
let isolate = new ivm.Isolate({ memoryLimit: 32, snapshot });
for (let ii = 0; ii < 500; ++ii) {
	let context = isolate.createContextSync();
	let global = context.global;
	context.release();
}
console.log('pass');

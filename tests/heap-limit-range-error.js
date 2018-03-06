'use strict';
let ivm = require('isolated-vm');

let isolate = new ivm.Isolate({ memoryLimit: 16 });
let context = isolate.createContextSync();
context.global.setSync('copy', new ivm.ExternalCopy([ new ArrayBuffer(1024 * 1024 * 16) ]));
try {
	isolate.compileScriptSync('copy.copy()').runSync(context);
	console.log('did not fail?');
} catch (err) {
	if (err instanceof RangeError) {
		console.log('pass');
	} else {
		console.log('got error', err);
	}
}

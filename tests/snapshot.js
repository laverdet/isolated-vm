"use strict";
let ivm = require('isolated-vm');
let snapshot = ivm.Isolate.createSnapshot([ { code: 'function sum(a,b) { return a + b }' } ]);
let isolate = new ivm.Isolate({ snapshot });
try {
	snapshot.copy({ transferIn: true });
	console.log('buffer transferred');
} catch (err) {
	if (!/in use/.test(err)) {
		console.log('weird error');
	}
}
let context = isolate.createContextSync();
console.log(isolate.compileScriptSync('sum(1, 2)').runSync(context) === 3 ? 'pass' : 'fail');

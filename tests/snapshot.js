"use strict";
const ivm = require('isolated-vm');
const assert = require('assert');

let code = 'function sum(a,b) { return a + b }\n';
if (process.versions.modules >= 64) {
	code += 'const array = new Uint32Array(128); array[100] = 0xdeadbeef;\n';
}
let snapshot = ivm.Isolate.createSnapshot([ { code } ]);
let isolate = new ivm.Isolate({ snapshot });
/* try {
	snapshot.copy({ transferIn: true });
	assert.fail('buffer transferred');
} catch (err) {
	assert.ok(/in use/.test(err), 'weird error');
} */
let context = isolate.createContextSync();
assert.equal(isolate.compileScriptSync('sum(1, 2)').runSync(context), 3);
if (process.versions.modules >= 64) {
	assert.equal(isolate.compileScriptSync('array[100]').runSync(context), 0xdeadbeef);
}

{
	let snapshot = ivm.Isolate.createSnapshot([{
		code: `utf8 = 'è';`
	}]);
	let isolate = new ivm.Isolate({ snapshot });
	let context = isolate.createContextSync();
	assert.equal(context.global.getSync('utf8'), 'è');
}

console.log('pass');

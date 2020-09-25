const { V8_AT_LEAST } = require('./lib/v8-version');
const assert = require('assert');
const ivm = require('isolated-vm');

if (V8_AT_LEAST(8, 0, 160)) {
	const isolate = new ivm.Isolate;
	const context = isolate.createContextSync();
	context.evalSync('eval("1".repeat(40).repeat(1e3))');
	assert.throws(() => context.evalSync('eval("1".repeat(40).repeat(1e7))'));
}
console.log('pass');

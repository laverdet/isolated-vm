const ivm = require('isolated-vm');
const assert = require('assert');

const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
const global = context.global;
global.setSync('global', global.derefInto());

const value = { foo: 'bar' };

global.setSync('test', value, { reference: true });
assert.equal(global.getSync('test').deref(), value);

global.setSync('test', value, { copy: true });
assert.deepEqual(global.getSync('test', { copy: true }), value);
assert.deepEqual(global.getSync('test', { externalCopy: true }).copy(), value);
global.getSync('test', { copy: true, promise: true }).then(resolved => assert.deepEqual(resolved, value)).then(resolved);

assert.deepEqual(isolate.compileScriptSync('test').runSync(context, { copy: true }), value);
const fn = isolate.compileScriptSync('(function foo(arg1) { return arg1; })').runSync(context, { reference: true });
assert.deepEqual(fn.applySync(undefined, [ value ], { arguments: { copy: true }, return: { copy: true } }), value);

const promise = isolate.compileScriptSync(
	'global.promise = new Promise(resolve => { global.resolve = resolve; })'
).runSync(context, { copy: true, promise: true });
global.getSync('promise', { copy: true, promise: true }).then(resolved => assert.deepEqual(value, resolved)).then(resolved);
global.getSync('resolve').applySync(undefined, [ value ], { arguments: { copy: true } });
promise.then(resolved => assert.deepEqual(value, resolved)).then(resolved);

isolate.compileScriptSync('new Promise(() => {})').runSync(context, { promise: true }).catch(resolved);
isolate.dispose();

let ii = 0;
function resolved() {
	if (++ii === 4) {
		console.log('pass');
	}
}

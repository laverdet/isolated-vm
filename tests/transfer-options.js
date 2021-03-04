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
assert.deepEqual(context.evalSync('test', { copy: true }), value);

const fn = context.evalSync('(function foo(arg1) { return arg1; })', { reference: true });
assert.deepEqual(fn.applySync(undefined, [ value ], { arguments: { copy: true }, result: { copy: true } }), value);

const closureResult = context.evalClosureSync('return [ $0, $1 ];', [ value, value ], { arguments: { copy: true }, result: { copy: true } });
assert.deepEqual(closureResult[0], closureResult[1]);
assert.deepEqual(closureResult[0], value);

const promise = isolate.compileScriptSync(
	'global.promise = new Promise(resolve => { global.resolve = resolve; })'
).runSync(context, { copy: true, promise: true });
global.getSync('promise', { copy: true, promise: true }).then(resolved => assert.deepEqual(value, resolved)).then(resolved);
global.getSync('resolve', { reference: true }).applySync(undefined, [ value ], { arguments: { copy: true } });
promise.then(resolved => assert.deepEqual(value, resolved)).then(resolved);

assert.throws(() => context.evalClosureSync(`return {}`));

const delegatedPromise = context.evalClosureSync(
	`return $0.apply(undefined, [], { result: { promise: true, copy: true }})`,
	[ () => new Promise(resolve => process.nextTick(() => resolve(value))) ],
	{ arguments: { reference: true }, result: { promise: true, copy: true } });
delegatedPromise.then(resolved => assert.deepEqual(resolved, value)).then(resolved);

context.evalClosure(
	'return $0.applySyncPromise()',
	[ async() => new ivm.ExternalCopy(value).copyInto() ], { arguments: { reference: true }, result: { copy: true }}
).then(result => assert.deepEqual(result, value));

let ii = 0;
function resolved() {
	if (++ii === 4) {
		isolate.compileScriptSync('new Promise(() => {})').runSync(context, { promise: true }).catch(() => {
			// abandoned
			console.log('pass');
		});
		isolate.dispose();
	}
}

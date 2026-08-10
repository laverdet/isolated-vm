const ivm = require('isolated-vm');
const assert = require('assert');

(async function() {
	// Without a handler the rejection is thrown out of whichever call is running at the time
	const bare = new ivm.Isolate;
	const bareContext = await bare.createContext();
	await assert.rejects(
		bareContext.eval(`Promise.reject(new Error('nobody listens'))`),
		/nobody listens/);
	bare.dispose();

	// A rejection which surfaces between calls is thrown out of the next call, however unrelated
	const plain = new ivm.Isolate;
	const plainContext = await plain.createContext();
	await plainContext.evalClosure(
		`globalThis.run = () => {
			void $0.apply(undefined, [], { result: { promise: true } }).then(() => { throw new Error('late'); });
		}`,
		[ () => new Promise(resolve => setTimeout(resolve, 20)) ],
		{ arguments: { reference: true } });
	await plainContext.eval('run()');
	await new Promise(resolve => setTimeout(resolve, 100));
	await assert.rejects(plainContext.eval('1 + 1'), /late/);
	plain.dispose();

	// With a handler the calls are left alone and every rejected value reaches the handler
	const seen = [];
	const isolate = new ivm.Isolate({ onUnhandledRejection: error => seen.push(error) });
	const context = await isolate.createContext();
	await assert.doesNotReject(
		context.eval(`Promise.reject(new Error('first')); Promise.reject(new Error('second'))`));
	await context.evalClosure(
		`globalThis.run = () => {
			void $0.apply(undefined, [], { result: { promise: true } }).then(() => { throw new Error('late'); });
		}`,
		[ () => new Promise(resolve => setTimeout(resolve, 20)) ],
		{ arguments: { reference: true } });
	await context.eval('run()');
	await new Promise(resolve => setTimeout(resolve, 100));
	assert.strictEqual(await context.eval('1 + 1'), 2);
	isolate.dispose();

	await new Promise(resolve => setTimeout(resolve, 100));
	assert.deepStrictEqual(seen.map(error => error.message), [ 'first', 'second', 'late' ]);

	// A handler which throws has nobody left to report to, so it is ignored
	const throwing = new ivm.Isolate({ onUnhandledRejection: () => { throw new Error('from the handler') } });
	const throwingContext = await throwing.createContext();
	await assert.doesNotReject(throwingContext.eval(`Promise.reject(new Error('third'))`));
	await new Promise(resolve => setTimeout(resolve, 100));
	assert.strictEqual(await throwingContext.eval('1 + 1'), 2);
	throwing.dispose();

	// A handler may live in another isolate rather than in node and receives the value the same way
	const outer = new ivm.Isolate;
	const outerContext = await outer.createContext();
	outerContext.global.setSync('ivm', ivm);
	await outerContext.eval(`(async () => {
		globalThis.seen = [];
		const inner = new ivm.Isolate({ onUnhandledRejection: error => seen.push(error.message) });
		const innerContext = await inner.createContext();
		await innerContext.eval('Promise.reject(new Error("nested"))');
		inner.dispose();
	})()`, { promise: true });
	await new Promise(resolve => setTimeout(resolve, 100));
	assert.strictEqual(await outerContext.eval('seen.join()'), 'nested');
	outer.dispose();

	console.log('pass');
})().catch(console.error);

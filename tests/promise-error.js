// node-args: --expose-gc

const ivm = require('isolated-vm');
const assert = require('assert');
const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
(async function() {
	assert.throws(() =>
		isolate.compileScriptSync('Promise.resolve().then(() => { throw new Error("hello") })').runSync(context),
		/hello/);

	await assert.rejects(
		(await isolate.compileScript('Promise.resolve().then(() => { throw new Error("hello") })')).run(context),
		/hello/);

	await assert.doesNotReject(
		context.eval('Promise.race([ 1, 2 ])', { promise: true }));


	await assert.rejects(() =>
		context.evalSync(`Promise.reject(new Error('hello'))`, { promise: true }),
		/hello/);

	assert.doesNotThrow(() =>
		context.evalSync('Promise.reject(new Error(1)).catch(() => {})'));

	assert.doesNotThrow(() => context.evalSync(`
		(async () => {
			try {
				await new Promise((_, reject) => reject(new Error('abc')));
			} catch (err) {
				// ignore
			}
		})();
	`));

	await assert.doesNotReject(async () => {
		const isolate = new ivm.Isolate;
		for (let i = 0; i < 20; i++) {
			const context = await isolate.createContext();
			await context.eval('const value = new Uint8Array(1024 * 1024 * 16); Promise.reject();').catch(() => {});
			context.release();
			global.gc();
		}
	});

	try {
		context.evalSync(`
			function innerFunction() {
				throw new Error("hello");
			}
			Promise.resolve().then(innerFunction);
		`);
		assert.ok(false);
	} catch (err) {
		assert.ok(/innerFunction/.test(err.stack));
	}

	console.log('pass');
})().catch(console.error);

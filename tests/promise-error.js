const ivm = require('isolated-vm');
const assert = require('assert');
const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
(async function() {
	assert.throws(() => {
		isolate.compileScriptSync('Promise.resolve().then(() => { throw new Error("hello") })').runSync(context);
	}, /hello/);

	assert.rejects(async() => {
		await (await isolate.compileScript('Promise.resolve().then(() => { throw new Error("hello") })')).run(context);
	}, /hello/);

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

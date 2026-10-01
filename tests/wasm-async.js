const ivm = require('isolated-vm');
const assert = require('assert');

(async function() {
	const isolate = new ivm.Isolate({ memoryLimit: 64 });
	const context = await isolate.createContext();
	const bytes = [0,97,115,109,1,0,0,0,1,7,1,96,2,127,127,1,127,3,2,1,0,7,7,1,3,97,100,100,0,0,10,9,1,7,0,32,0,32,1,106,11];
	assert.strictEqual(WebAssembly.validate(new Uint8Array(bytes)), true);
	const watchdog = setTimeout(() => {
		throw new Error('Async WebAssembly task did not settle');
	}, 3000);
	try {
		assert.strictEqual(await context.eval(`
			WebAssembly.instantiate(new Uint8Array(${JSON.stringify(bytes)}))
				.then(result => result.instance.exports.add(5, 6));
		`, { promise: true, copy: true, timeout: 1000 }), 11);
		assert.strictEqual(await context.eval(`
			WebAssembly.compile(new Uint8Array(${JSON.stringify(bytes)}))
				.then(module => new WebAssembly.Instance(module).exports.add(5, 6));
		`, { promise: true, copy: true, timeout: 1000 }), 11);
		assert.strictEqual(await context.eval(`
			WebAssembly.instantiate(new Uint8Array([0]))
				.then(() => 'unexpected success', error => error.name);
		`, { promise: true, copy: true, timeout: 1000 }), 'CompileError');
		assert.strictEqual(await context.eval('1 + 1'), 2);
	} finally {
		clearTimeout(watchdog);
		isolate.dispose();
	}
	console.log('pass');
})().catch(error => {
	console.error(error);
	process.exitCode = 1;
});

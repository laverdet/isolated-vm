'use strict';
const ivm = require('isolated-vm');
const assert = require('assert');
const { spawnSync } = require('child_process');

async function run(mode) {
	const isolate = new ivm.Isolate({ memoryLimit: 64 });
	const context = isolate.createContextSync();
	try {
		if (mode === 'budget') {
			const started = Date.now();
			await assert.rejects(context.eval(`
				let chain = WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0]));
				for (let ii = 0; ii < 2000; ++ii) {
					chain = chain.then(() => {
						const until = Date.now() + 10;
						while (Date.now() < until) {}
					});
				}
				chain;
			`, { promise: true, timeout: 200 }), /Script execution timed out/);
			assert.ok(Date.now() - started < 2000, 'The original deadline was reset between continuations');
		} else if (mode === 'idle') {
			await assert.rejects(context.eval('new Promise(() => {})', { promise: true, timeout: 100 }), /Script execution timed out/);
		} else if (mode === 'ignored') {
			context.evalSync('WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0])).then(() => { while (true) {} }); undefined;', { timeout: 100 });
			await new Promise(resolve => setTimeout(resolve, 300));
		} else if (mode === 'shared') {
			context.evalSync('promise = WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0])).then(() => { while (true) {} });', { timeout: 100 });
			const first = context.global.getSync('promise', { promise: true });
			const second = context.global.getSync('promise', { promise: true });
			await Promise.all([
				assert.rejects(first, /Script execution timed out/),
				assert.rejects(second, /Script execution timed out/),
			]);
		} else {
			const pending = mode === 'async'
				? context.eval('WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0])).then(() => { while (true) {} })', { promise: true, timeout: 100 })
				: context.evalSync('WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0])).then(() => { while (true) {} })', { promise: true, timeout: 100 });
			await assert.rejects(pending, /Script execution timed out/);
		}
		assert.strictEqual(await context.eval('1 + 1'), 2);
	} finally {
		if (!isolate.isDisposed) isolate.dispose();
	}
}

if (process.argv[2]) {
	run(process.argv[2]).then(() => console.log('pass')).catch(error => {
		console.error(error);
		process.exitCode = 1;
	});
} else {
	for (const mode of [ 'async', 'sync', 'budget', 'idle', 'ignored', 'shared' ]) {
		const child = spawnSync(process.execPath, [ '--no-node-snapshot', __filename, mode ], {
			encoding: 'utf8', timeout: 10000,
		});
		assert.ifError(child.error);
		assert.strictEqual(child.status, 0, `${mode}: ${child.stderr}`);
		assert.strictEqual(child.stderr, '', mode);
		assert.strictEqual(child.stdout, 'pass\n', mode);
	}
	console.log('pass');
}

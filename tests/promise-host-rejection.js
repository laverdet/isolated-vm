// node-args: --unhandled-rejections=strict
const ivm = require('isolated-vm');
const assert = require('assert');
const { spawnSync } = require('child_process');

(async function() {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	const watchdog = setTimeout(() => {
		throw new Error('Host promise transfer did not settle');
	}, 5000);
	try {
		const cases = [
			[ () => Promise.reject(new Error('immediate rejection')), 'immediate rejection' ],
			[ async () => { throw new Error('async rejection'); }, 'async rejection' ],
			[ () => Promise.reject('primitive rejection'), 'primitive rejection' ],
			[ () => new Promise((resolve, reject) => setImmediate(() => reject(new Error('delayed rejection')))), 'delayed rejection' ],
			[ () => { throw new Error('synchronous throw'); }, 'synchronous throw' ],
		];
		for (const method of [ 'apply', 'applySync' ]) {
			for (const [ callback, expected ] of cases) {
				const reference = new ivm.Reference(callback);
				try {
					const result = await context.evalClosure(`
						return (async () => {
							try {
								await $0.${method}(undefined, [], { result: { promise: true, copy: true } });
								return 'unexpected success';
							} catch (error) {
								return error instanceof Error ? error.message : error;
							}
						})();
					`, [ reference ], { result: { promise: true, copy: true }, timeout: 1000 });
					assert.strictEqual(result, expected);
					await new Promise(resolve => setImmediate(resolve));
				} finally {
					reference.release();
				}
			}
		}
		assert.strictEqual(await context.evalClosure(
			'return $0.apply(undefined, [], { result: { promise: true, copy: true } });',
			[ () => Promise.resolve('fulfilled') ],
			{ arguments: { reference: true }, result: { promise: true, copy: true } }), 'fulfilled');
		assert.strictEqual(await context.eval('1 + 1'), 2);

		const unhandled = spawnSync(process.execPath, [
			'--no-node-snapshot', '--unhandled-rejections=strict', '-e',
			"const ivm = require('isolated-vm'); new ivm.Isolate; Promise.reject(new Error('unrelated host rejection'));",
		], { encoding: 'utf8', timeout: 5000 });
		assert.ifError(unhandled.error);
		assert.strictEqual(unhandled.status, 1);
		assert.match(unhandled.stderr, /unrelated host rejection/);
	} finally {
		clearTimeout(watchdog);
		isolate.dispose();
	}
	console.log('pass');
})().catch(error => {
	console.error(error);
	process.exitCode = 1;
});

const ivm = require('isolated-vm');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { once } = require('events');
const { Worker, isMainThread, parentPort } = require('worker_threads');

function leb(value) {
	const bytes = [];
	do {
		const byte = value & 127;
		value >>>= 7;
		bytes.push(byte | (value ? 128 : 0));
	} while (value);
	return bytes;
}

function moduleBytes(count) {
	const section = (id, bytes) => [ id, ...leb(bytes.length), ...bytes ];
	const functions = [ ...leb(count), ...new Array(count).fill(0) ];
	const code = [ ...leb(count) ];
	for (let index = 0; index < count; ++index) {
		code.push(4, 0, 65, index % 64, 11);
	}
	return new Uint8Array([ 0,97,115,109,1,0,0,0,
		...section(1, [ 1,96,0,1,127 ]), ...section(3, functions), ...section(10, code) ]);
}

async function run(mode) {
	if (!mode) {
		for (const childMode of [ 'idle', 'no-watchdog', 'dispose', 'worker' ]) {
			const child = spawnSync(process.execPath, [ '--no-node-snapshot', __filename, childMode ], {
				encoding: 'utf8', timeout: 15000,
			});
			assert.ifError(child.error);
			assert.strictEqual(child.status, 0, `${childMode}: ${child.stderr}`);
			assert.strictEqual(child.stderr, '', childMode);
			assert.strictEqual(child.stdout, 'pass\n', childMode);
		}
		return;
	}
	if (mode === 'worker') {
		for (const workerMode of [ 'no-watchdog', 'dispose' ]) {
			const worker = new Worker(__filename, { argv: [ workerMode ] });
			const messages = [];
			worker.on('message', message => messages.push(message));
			const [ code ] = await once(worker, 'exit');
			assert.strictEqual(code, 0);
			assert.deepStrictEqual(messages, [ 'pass' ]);
		}
		return;
	}
	const isolate = new ivm.Isolate({ memoryLimit: 128 });
	const context = isolate.createContextSync();
	const bytes = moduleBytes(4096);
	assert.strictEqual(WebAssembly.validate(bytes), true);
	context.global.setSync('bytes', new ivm.ExternalCopy(bytes).copyInto());
	const watchdog = mode === 'no-watchdog' ? undefined : setTimeout(() => {
		throw new Error(`Async WebAssembly ${mode} task did not settle`);
	}, 10000);
	try {
		if (mode === 'idle') {
			await new Promise(resolve => {
				context.global.setSync('done', new ivm.Callback(() => resolve(), { ignored: true }));
				context.evalSync('WebAssembly.compile(bytes).then(() => done()); undefined;');
			});
		} else if (mode === 'no-watchdog') {
			assert.strictEqual(await context.eval('WebAssembly.compile(bytes).then(() => 42)', { promise: true }), 42);
		} else if (mode === 'dispose') {
			const pending = context.evalSync('WebAssembly.compile(bytes).then(() => 42)', { promise: true });
			const rejected = assert.rejects(pending, /Promise was abandoned/);
			isolate.dispose();
			await rejected;
			await new Promise(resolve => setTimeout(resolve, 100));
			const sibling = new ivm.Isolate;
			try {
				assert.strictEqual(await sibling.createContext().then(context => context.eval('1 + 1')), 2);
			} finally {
				sibling.dispose();
			}
		}
	} finally {
		clearTimeout(watchdog);
		if (!isolate.isDisposed) isolate.dispose();
	}
}

run(process.argv[2]).then(() => {
	if (isMainThread) console.log('pass');
	else parentPort.postMessage('pass');
}).catch(error => {
	console.error(error);
	process.exitCode = 1;
});

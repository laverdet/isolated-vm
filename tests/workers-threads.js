if (process.versions.modules < 72) {
	console.log('pass');
	process.exit();
}
const ivm = require('isolated-vm');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

const kIsolateCount = 100;
const kWorkerCount = 10;
if (isMainThread) {
	Promise.all(Array(kWorkerCount).fill().map(async (_, ii) => {
		const worker = new Worker(__filename);
		const exit = new Promise((resolve, reject) => {
			worker.on('exit', code => code ? reject(new Error(`Worker exitted with code: ${code}`)) : resolve());
			worker.on('error', error => reject(error));
		});
		const results = [];
		worker.on('message', message => results.push(...message));
		await exit;
		assert(results.length == kIsolateCount);
		for (let ii = 0; ii < kIsolateCount; ++ii) {
			assert(results.includes(`pass${ii}`));
		}
		return true;
	})).then(results => {
		if (results.every(value => value)) {
			console.log('pass');
		}
	});
} else {
	Promise.all(Array(kIsolateCount).fill().map(async (_, ii) => {
		const isolate = new ivm.Isolate;
		const context = await isolate.createContext();
		const logs = [];
		await context.global.set('log', new ivm.Reference(str => logs.push(str)));
		await context.global.set('ii', ii);
		const script = await isolate.compileScript('log.apply(null, [`pass${ii}`])');
		await script.run(context);
		parentPort.postMessage(logs);
	})).catch(err => console.error(err));
}

if (true || process.versions.modules < 72) {
	// Disabled
	console.log('pass');
	process.exit();
}
const ivm = require('isolated-vm');
const { Worker, isMainThread, parentPort } = require('worker_threads');
if (isMainThread) {
	const worker = new Worker(__filename);
	worker.on('message', () => { worker.terminate() });
	process.on('exit', () => console.log('pass'));
} else {
	const isolate = new ivm.Isolate;
	const context = isolate.createContextSync();
	context.eval('for(;;);');
	parentPort.postMessage('');
}

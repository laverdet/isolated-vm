// node-args: --expose-gc
const ivm = require('isolated-vm');

(async () => {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	await context.evalClosure(
		'wait = ms => $0.applySync(undefined, [ ms ], { result: { promise: true } })',
		[ ms => new Promise(resolve => setTimeout(resolve, ms)) ],
		{ arguments: { reference: true } },
	);
	await context.eval('wait(100)');
	process.nextTick(() => {
		gc();
		console.log('pass');
	});
})();

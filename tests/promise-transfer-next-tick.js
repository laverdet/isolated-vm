const ivm = require('isolated-vm');

(async () => {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	await context.evalClosure(
		'sleep = ms => $0.applySync(undefined, [ ms ], { result: { promise: true } })',
		[ ms => new Promise(resolve => setTimeout(resolve, ms)) ],
		{ arguments: { reference: true } },
	);
	// `process.nextTick` callbacks queued by the continuation of a transferred promise must run
	// right away, the same as after any other node callback.
	setTimeout(() => {
		console.log('fail');
		process.exit();
	}, 10000);
	await context.eval('sleep(5)', { promise: true });
	process.nextTick(() => {
		console.log('pass');
		process.exit();
	});
})();

const ivm = require('isolated-vm');

(async () => {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	await context.evalClosure(
		'sleep = ms => $0.applySync(undefined, [ ms ], { result: { promise: true } })',
		[ ms => new Promise(resolve => setTimeout(resolve, ms)) ],
		{ arguments: { reference: true } },
	);
	// The transferred promise must settle on its own, without waiting for
	// another callback to wake the microtask queue.
	setTimeout(() => {
		console.log('fail');
		process.exit();
	}, 10000);
	await context.eval('sleep(5)', { promise: true });
	console.log('pass');
	process.exit();
})();

const ivm = require('isolated-vm');

(async () => {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	// A host function that defers part of its work with `process.nextTick`, like many I/O clients.
	await context.global.set('host', new ivm.Reference(() => new Promise(resolve => process.nextTick(resolve))));
	const script = await isolate.compileScript(`(async () => {
		for (let i = 0; i < 3; i++) {
			await host.apply(undefined, [], { result: { promise: true } });
		}
	})()`);
	// Ticks queued while a task runs in the nodejs isolate must run right away, the same as after
	// any other node callback.
	setTimeout(() => {
		console.log('fail');
		process.exit();
	}, 10000);
	await script.run(context, { promise: true });
	console.log('pass');
	process.exit();
})();

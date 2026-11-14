'use strict';
if (process.platform === 'win32') {
	// No SIGCHLD to cause the spurious wakeups
	console.log('pass');
	process.exit();
}
const ivm = require('isolated-vm');
const { execFile } = require('child_process');

// The thread blocked in `applySyncPromise` may wake spuriously, see the result, and return before
// the default thread has notified it. Here the SIGCHLD from each child process provides the
// spurious wakeups. Notifying after the waiting thread returned wrote into its reused stack, which
// crashed in whatever ran next: here, the lazy compilation of the new functions.
(async function() {
	const deadline = Date.now() + 5000;
	await Promise.all(Array.from({ length: 4 }, async () => {
		const isolate = new ivm.Isolate({ memoryLimit: 32 });
		const context = await isolate.createContext();
		await context.evalClosure(
			'run = $0',
			[ () => new Promise(resolve => execFile('true', () => resolve('ok'))) ],
			{ arguments: { reference: true } },
		);
		for (let ii = 0; Date.now() < deadline; ++ii) {
			const fns = Array.from({ length: 20 }, (_, jj) => `x => x.slice(0, ${jj + 1}).length + ${ii}`);
			await context.eval(`(() => {
				const result = run.applySyncPromise(undefined, []);
				return [ ${fns.join(', ')} ].map(fn => fn(result));
			})()`, { copy: true });
		}
		isolate.dispose();
	}));
	console.log('pass');
})().catch(console.error);

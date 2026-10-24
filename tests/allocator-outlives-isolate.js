// node-args: --expose-gc
'use strict';
const ivm = require('isolated-vm');

// A SharedArrayBuffer leaves its isolate shared rather than copied, and its backing store keeps
// the isolate's allocator alive. Freeing it after the isolate is disposed must not write to the
// disposed isolate's memory accounting, which the next isolate is likely to occupy.
(async function() {
	for (let ii = 0; ii < 10; ++ii) {
		const isolate = new ivm.Isolate({ memoryLimit: 32 });
		let view = isolate.createContextSync().evalSync('new Uint8Array(new SharedArrayBuffer(1024 * 1024))', { copy: true });
		isolate.dispose();
		await new Promise(resolve => setTimeout(resolve, 5));
		const next = new ivm.Isolate({ memoryLimit: 32 });
		const before = next.getHeapStatisticsSync().externally_allocated_size;
		view = undefined;
		gc();
		await new Promise(resolve => setTimeout(resolve, 5));
		const after = next.getHeapStatisticsSync().externally_allocated_size;
		next.dispose();
		if (after !== before) {
			console.log(`externally_allocated_size ${before} -> ${after}`);
			return;
		}
	}
	console.log('pass');
})().catch(console.error);

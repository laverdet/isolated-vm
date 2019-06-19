'use strict';
const ivm = require('isolated-vm');
const memoryLimit = 32;
function makeIsolate(memoryLimit) {
	let isolate = new ivm.Isolate({ memoryLimit });
	return { isolate, context: isolate.createContextSync() };
}

// Figure out how much junk gets us to the memory limit
let env = makeIsolate(memoryLimit);
let count;
env.context.global.setSync('update', new ivm.Reference(function(ii) {
	let heap = env.isolate.getHeapStatisticsSync();
	if (heap.used_heap_size < memoryLimit * 1024 * 1024) {
		count = ii;
	}
}));

try {
	env.isolate.compileScriptSync(`
		let list = [];
		for (let ii = 0;; ++ii) {
			list.push({});
			if (ii % 1000 === 0) {
				update.applySync(undefined, [ ii ]);
			}
		}
	`).runSync(env.context);
} catch (err) {};

// Try to allocate an array while garbage can be collected
let env2 = makeIsolate(memoryLimit);
env2.context.global.setSync('count', count);
env2.context.global.setSync('update', new ivm.Reference(function() {}));
env2.isolate.compileScriptSync(`
	let list = [];
	for (let ii = 0; ii < count; ++ii) {
		list.push({});
		if (ii % 1000 === 0) {
			// Called to better match GC of previous run
			update.applySync(undefined, [ ii ]);
		}
	}
	list = undefined;
	new Uint8Array(8 * 1024 * 1024);
`).runSync(env2.context);
console.log('pass');

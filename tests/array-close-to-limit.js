'use strict';
let ivm = require('isolated-vm');
function makeIsolate() {
	let isolate = new ivm.Isolate({ memoryLimit: 8 });
	return { isolate, context: isolate.createContextSync() };
}

let count;
let update = new ivm.Reference(function(ii) {
	count = ii;
});

// Figure out how much junk gets us to the memory limit
let env = makeIsolate();
env.context.global.setSync('update', update);
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
let env2 = makeIsolate();
env2.context.global.setSync('count', count);
env2.isolate.compileScriptSync(`
	let list = [];
	while (--count) {
		list.push({});
	}
	list = undefined;
	new Uint8Array(1024 * 1024);
`).runSync(env2.context);
console.log('pass');

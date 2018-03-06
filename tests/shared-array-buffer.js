'use strict';
// node-args: --harmony_sharedarraybuffer
let ivm = require('isolated-vm');

let buffer = new SharedArrayBuffer(1024 * 1024);
let array = new Uint8Array(buffer);

function runSync(env, fn) {
	env.isolate.compileScriptSync('new '+ fn).runSync(env.context);
}

function makeIsolate() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.global;
	return { isolate, context, global };
}
let env = makeIsolate();

// Check initial transfer
env.global.setSync('buffer', new ivm.ExternalCopy(buffer).copyInto({ release: true }));
env.global.setSync('ivm', ivm);

if (env.isolate.getHeapStatisticsSync().externally_allocated_size !== 1024 * 1024) {
	console.log('size wrong');
}

// Check that buffer transfer + externalize size adjustments work
let env2 = makeIsolate();
env.global.setSync('global2', env2.global);
env2.global.setSync('ivm', ivm);
runSync(env, function() {
	global2.setSync('buffer', new ivm.ExternalCopy(buffer).copyInto({ release: true }));
});

if (
	env.isolate.getHeapStatisticsSync().externally_allocated_size !== 1024 * 1024 ||
	env2.isolate.getHeapStatisticsSync().externally_allocated_size !== 1024 * 1024
) {
	console.log('size wrong2');
}

// Make sure we're using the same array
env.global.setSync('array', new ivm.ExternalCopy(array).copyInto({ release: true }));
runSync(env, function() {
	array[0] = 123;
});
runSync(env2, function() {
	let foo = new Uint8Array(buffer);
	foo[1] = 234;
	buf2 = new SharedArrayBuffer(1024 * 1024);
	new ivm.ExternalCopy(buf2); // externalize
});

if (array[0] !== 123 || array[1] !== 234) {
	console.log('not shared');
}

if (env.isolate.getHeapStatisticsSync().externally_allocated_size !== 1024 * 1024 * 2) {
	// This isn't really a good thing but I can't really think of an efficient way to fix it
	console.log('size wrong3');
}
if (env2.isolate.getHeapStatisticsSync().externally_allocated_size !== 1024 * 1024 * 2) {
	console.log('size wrong4');
}

console.log('pass');

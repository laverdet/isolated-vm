'use strict';
// node-args: --expose-gc
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 32 });
let context = isolate.createContextSync();
let global = context.global;

let kb1 = '................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................';
let kb2 = '\uffff................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................';

// Sanity check
if (ivm.ExternalCopy.totalExternalSize) {
	console.log('has size already??');
}

// Check one byte string
let copy1 = new ivm.ExternalCopy(kb1);
if (ivm.ExternalCopy.totalExternalSize < 1024) {
	console.log('1kb small');
}

// Check two byte string
let copy2 = new ivm.ExternalCopy(kb2);
if (ivm.ExternalCopy.totalExternalSize < 1024 + 2048) {
	console.log('2kb small');
}

// Dispose and verify they were collected. Do this here to ensure no v8 handles.
copy1.release();
copy2.release();
if (ivm.ExternalCopy.totalExternalSize) {
	console.log('external copy size is ', ivm.ExternalCopy.totalExternalSize);
}

// Remake the copies
copy1 = new ivm.ExternalCopy(kb1);
copy2 = new ivm.ExternalCopy(kb2);

// Verify results
let internal1 = copy1.copy();
if (internal1 !== kb1) {
	console.log('1kb bad copy');
}

let internal2 = copy2.copy();
if (internal2 !== kb2) {
	conosle.log('2kb bad copy');
}

// Verify isolate heap
if (isolate.getHeapStatisticsSync().externally_allocated_size) {
	console.log('isolate already has heap?');
}
global.setSync('a', copy1.copyInto());
if (isolate.getHeapStatisticsSync().externally_allocated_size < 1024) {
	console.log('isolate did not externalize');
}

// Force garbage collection, hopefully. Check string was collected
global.setSync('a', null);
isolate.compileScriptSync('gc()').runSync(context);
if (isolate.getHeapStatisticsSync().externally_allocated_size) {
	console.log('string was not collected. this may not be a bug');
}

// Hit isolate memory limit with external strings
try {
	for (let ii = 0; ii < 32 * 1024; ++ii) {
		global.setSync(ii, copy2.copyInto());
	}
	console.log('did not hit limit', isolate.getHeapStatisticsSync());
} catch (err) {
	console.log('pass');
}

// Verify internalized copies
if ((new ivm.ExternalCopy(kb1.substr(0, 100))).copy() !== kb1.substr(0, 100)) {
	console.log('kb1 slice mismatch');
}
if ((new ivm.ExternalCopy(kb2.substr(0, 100))).copy() !== kb2.substr(0, 100)) {
	console.log('kb2 slice mismatch');
}

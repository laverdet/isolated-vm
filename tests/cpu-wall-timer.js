'use strict';
const kTimeout = 100;
const kTimeoutRange = 115;
const kError = 0.98;
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();

function spin(timeout) {
	let d = Date.now() + timeout + 2;
	while (Date.now() < d);
}

function hrToFloat(hr) {
	return Number(hr) / 1e6;
}

function checkRange(val, min, max) {
	return val <= max / kError && val >= min * kError;
}

// Synchronous timers
context.global.setSync('defaultIsolateSpin', new ivm.Reference(spin));
context.global.setSync('recurse', new ivm.Reference(function(code) {
	isolate.compileScriptSync(code).runSync(context);
}));
isolate.compileScriptSync(''+ spin).runSync(context);
isolate.compileScriptSync(`
	spin(${kTimeout});
	defaultIsolateSpin.applySync(undefined, [ ${kTimeout} ]);
	recurse.applySync(undefined, [ 'spin(${kTimeout});' ]);
`).runSync(context);

if (!checkRange(hrToFloat(isolate.cpuTime), kTimeout * 2, kTimeoutRange * 2)) {
	console.log('cpu time wrong');
}
if (!checkRange(hrToFloat(isolate.wallTime), kTimeout * 3, kTimeoutRange * 3)) {
	console.log('wall time wrong');
}

// Asynchronous time
let initialCpu = hrToFloat(isolate.cpuTime);
let lastCpu;
let initialWall = hrToFloat(isolate.wallTime);
isolate.compileScriptSync(`spin(${kTimeout}); defaultIsolateSpin.applySync(undefined, [ ${kTimeout} ] );`).run(context);
let d = Date.now() + kTimeoutRange * 2;
let passedCpu = false;
while (Date.now() < d && !passedCpu) {
	let cpuTime = hrToFloat(isolate.cpuTime);
	// Ensures cpu is ticking up while the spin() loop runs in another thread
	if (lastCpu === undefined) {
		lastCpu = cpuTime;
	} else if (lastCpu !== cpuTime) {
		passedCpu = true;
	}
}
if (!passedCpu) {
	console.log('Async CPU time not updating');
}

d = Date.now() + kTimeout * 4;
let interval = setInterval(function() {
	if (Date.now() > d) {
		clearInterval(interval);
	}
	if (checkRange(hrToFloat(isolate.wallTime) - initialWall, kTimeout * 2, kTimeoutRange * 2)) {
		clearInterval(interval);
		if (checkRange(hrToFloat(isolate.cpuTime) - initialCpu, kTimeout, kTimeoutRange)) {
			console.log('pass');
		}
	}
}, 10);

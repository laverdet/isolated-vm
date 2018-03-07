'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();

function spin(timeout) {
	let d = Date.now() + timeout + 2;
	while (Date.now() < d);
}

function hrToFloat(hr) {
	let time = (hr[0] + hr[1] / 1e9) * 1000;
	if (time > 1000) {
		console.log('time is too high');
	}
	return time;
}

// Synchronous timers
context.global.setSync('defaultIsolateSpin', new ivm.Reference(spin));
context.global.setSync('recurse', new ivm.Reference(function(code) {
	isolate.compileScriptSync(code).runSync(context);
}));
isolate.compileScriptSync(''+ spin).runSync(context);
isolate.compileScriptSync(`
	spin(100);
	defaultIsolateSpin.applySync(undefined, [ 100 ]);
	recurse.applySync(undefined, [ 'spin(100);' ]);
`).runSync(context);

if (Math.abs(hrToFloat(isolate.cpuTime)) - 200 > 8) {
	console.log('cpu time wrong');
}
if (Math.abs(hrToFloat(isolate.wallTime)) - 300 > 8) {
	console.log('wall time wrong');
}

// Asynchronous time
let initialCpu = hrToFloat(isolate.cpuTime);
let initialWall = hrToFloat(isolate.wallTime);
isolate.compileScriptSync('spin(100); defaultIsolateSpin.applySync(undefined, [ 100 ] );').run(context);
let d = Date.now() + 250;
let passedCpu = false;
let passedWall = false;
while (Date.now() < d && !(passedCpu && passedWall)) {
	let cpuTime = hrToFloat(isolate.cpuTime);
	if (initialCpu !== 0) {
		if (cpuTime !== initialCpu) {
			initialCpu = 0;
		}
	} else if (cpuTime > 300) {
		passedCpu = true;
	}

	let wallTime = hrToFloat(isolate.wallTime);
	if (initialWall !== 0) {
		if (wallTime !== initialWall) {
			initialWall = 0;
		}
	} else if (wallTime > 500) {
		passedWall = true;
	}
}
if (!passedCpu || hrToFloat(isolate.cpuTime) > 350) {
	console.log('async cpu failed');
}
if (!passedWall || hrToFloat(isolate.wallTime) > 550) {
	console.log('async wall failed');
}
console.log('pass');

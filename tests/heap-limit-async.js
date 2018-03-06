'use strict';
let ivm = require('isolated-vm');

// This is our code which will blow the v8 heap
let src = `
	let storage = [];
	let s = 'x';
	while (true) {
		s += Math.random();
		storage.push(s);
		wait.apply(null, [ s.length ]);
	}
`;

// First run is to figure out about how long it takes
let time = Date.now();
let isolate = new ivm.Isolate({ memoryLimit: 16 });
let context = isolate.createContextSync();
context.global.setSync('wait', new ivm.Reference(function() {}));
isolate.compileScriptSync(src).run(context).catch(function() {
	let timeTaken = Date.now() - time;

	// Now run again and ensure each `wait` finishes after the isolate is destroyed
	let runUntil = Date.now() + timeTaken * 2;
	let isolate = new ivm.Isolate({ memoryLimit: 16 });
	let context = isolate.createContextSync();
	context.global.setSync('wait', new ivm.Reference(function(a) {
		while (Date.now() < runUntil);
	}));
	isolate.compileScriptSync(src).run(context).catch(function(err) {
		if (/disposed/.test(err)) {
			setTimeout(() => console.log('pass'), 100);
		}
	});
});

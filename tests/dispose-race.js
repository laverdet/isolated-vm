'use strict';
let ivm = require('isolated-vm');

function make() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.global;
	let script = isolate.compileScriptSync('isolate.dispose()');
	return { isolate, context, global, script };
}

function race() {
	return new Promise(function(resolve) {
		let isolate1 = make();
		let isolate2 = make();

		isolate1.global.setSync('handle', isolate2.global);
		isolate1.global.setSync('isolate', isolate2.isolate);
		isolate2.global.setSync('handle', isolate1.global);
		isolate2.global.setSync('isolate', isolate1.isolate);

		let promises = [
			isolate1.script.run(isolate1.context),
			isolate2.script.run(isolate2.context),
		];
		let ii = promises.length;
		function update() {
			if (--ii === 0) {
				resolve();
			}
		}
		promises.forEach(promise => promise.then(update).catch(update));
	});
}

Promise.all([ Array(100).fill().map(() => race()) ]).then(() => console.log('pass'));

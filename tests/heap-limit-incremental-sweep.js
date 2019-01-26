'use strict';
const ivm = require('isolated-vm');

{
	const isolate = new ivm.Isolate();
	const context = isolate.createContextSync();
	context.global.setSync('ivm', ivm);

	const func = isolate.compileScriptSync('new ivm.Reference(function test(data) { return data.length; })').runSync(context);
	let counter = 0;

	for (let i = 0; i < 500; i++) {
		counter += func.applySync(null, [new ivm.ExternalCopy('x'.repeat(400000)).copyInto()]);
	}

	isolate.dispose();
}

{
	const isolate = new ivm.Isolate();
	const context = isolate.createContextSync();
	context.global.setSync('ivm', ivm);

	const func = isolate.compileScriptSync('new ivm.Reference(function test(data) { return data.text.length; })').runSync(context);
	let counter = 0;

	for (let i = 0; i < 500; i++) {
		counter += func.applySync(null, [new ivm.ExternalCopy({ text: 'x'.repeat(400000) }).copyInto()]);
	}

	isolate.dispose();
}

console.log('pass');

'use strict';
const ivm = require('isolated-vm');
for (let ii = 0; ii < 500; ++ii) {
	const isolate = new ivm.Isolate();
	const context = isolate.createContextSync();
	isolate.compileScriptSync('1').runSync(context, { timeout: 100 });
	isolate.dispose();
}
console.log('pass');

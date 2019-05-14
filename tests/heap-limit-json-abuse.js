let ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 32 });
let context = isolate.createContextSync();
let global = context.global;
let count;
if (require('./lib/v8-version').V8_AT_LEAST(6, 7, 185)) {
	count = Math.floor(1024 * 1024 * 25 / 12);
} else {
	count = Math.floor(1024 * 1024 * 32 / 12);
}
global.setSync('blob', new ivm.ExternalCopy({ val: `[${Array(count).fill('"aa"').join(',')}]` }).copyInto());

try {
	isolate.compileScriptSync('new '+ function() {
		JSON.parse(blob.val);
	}).runSync(context);
} catch (err) {
	console.log('pass');
}

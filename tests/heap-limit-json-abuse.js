let ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 32 });
let context = isolate.createContextSync();
let global = context.globalReference();
global.setSync('blob', new ivm.ExternalCopy({ val: `[${Array(Math.floor(1024 * 1024 * 32 / 12)).fill('"aa"').join(',')}]` }).copyInto());

try {
	isolate.compileScriptSync('new '+ function() {
		JSON.parse(blob.val);
	}).runSync(context);
} catch (err) {
	console.log('pass');
}

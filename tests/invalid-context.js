let ivm = require('isolated-vm');
let isolate1 = new ivm.Isolate();
let isolate2 = new ivm.Isolate();

try {
	isolate1.compileScriptSync('1').runSync(isolate2.createContextSync());
} catch (err) {
	console.log('pass');
}

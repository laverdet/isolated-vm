let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let global = context.global;

function recur1() {
	return global.getSync('recur2')();
}

global.setSync('recur1', new ivm.Reference(recur1));

console.log(isolate.compileScriptSync(`
function recur2() {
	return 'pass';
}
recur1.applySync(undefined, []);
`).runSync(context));

// For some reason this simple case triggered a bug that was due to unexpected ordering of static
// member destructors.
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let script = isolate.compileScriptSync('1');
script.runSync(context);
console.log('pass');

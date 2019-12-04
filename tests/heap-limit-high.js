const ivm = require('isolated-vm');

let isolate = new ivm.Isolate({ memoryLimit: 10 * 1024 * 1024 });
let context = isolate.createContextSync();

isolate.compileScriptSync(``).runSync(context);
console.log('pass');

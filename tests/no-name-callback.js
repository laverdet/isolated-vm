const ivm = require('isolated-vm');
const js = `Function.prototype.bind(Function.call, Object.prototype.hasOwnProperty);`;
const isolate = new ivm.Isolate();
const context = isolate.createContextSync();
const script = isolate.compileScriptSync(js);
script.runSync(context);
console.log('pass');

let ivm = require("isolated-vm");
let isolate = new ivm.Isolate;

let context;
isolate.createContext().then(val => context = val);
setTimeout(function() {
	console.log(context ? 'pass' : 'fail');
}, 100);

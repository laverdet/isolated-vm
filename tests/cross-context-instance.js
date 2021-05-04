let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;

function makeContext() {
	let context = isolate.createContextSync();
	let global = context.global;
	global.setSync('ivm', ivm);
	isolate.compileScriptSync(`
		function makeReference(ref) {
			return new ivm.Reference(ref);
		}
		function isReference(ref) {
			return ref instanceof ivm.Reference;
		}
	`).runSync(context);
	return {
		makeReference: global.getSync('makeReference'),
		isReference: global.getSync('isReference'),
	};
}

let context1 = makeContext();
let context2 = makeContext();
[ context1, context2 ].forEach(context => {
	if (!context.isReference(new ivm.Reference({}))) {
		console.log('fail1');
	}
	if (!context.isReference(context.makeReference({}))) {
		console.log('fail2');
	}
});
if (context1.isReference(context2.makeReference({}).derefInto())) {
	console.log('fail3');
}
console.log('pass');

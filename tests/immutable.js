const ivm = require('isolated-vm');
const { strictEqual, throws } = require('assert');

ivm.ExternalCopy = () => {};
Object.getPrototypeOf(ivm).ExternalCopy = () => {};

{
	ivm.ExternalCopy.prototype.copy = 1;
	const ref = new ivm.ExternalCopy(1);
	throws(() => Object.setPrototypeOf(ref, {}));
	strictEqual(ref.copy(), 1);
	strictEqual(Object.getPrototypeOf(ref), ivm.ExternalCopy.prototype);
}

{
	const isolate = new ivm.Isolate;
	const context = isolate.createContextSync();
	const result = context.evalClosureSync(`
		$0.__proto__.copy = 1;
		return $0.copy();
	`, [ new ivm.ExternalCopy(1) ]);
	strictEqual(result, 1);
}

{
	const isolate = new ivm.Isolate;
	const context = isolate.createContextSync();
	const result = context.evalClosureSync(`
		$0.ExternalCopy = () => {};
		$0.__proto__.ExternalCopy = () => {};
		return new $0.ExternalCopy(1).copy();
	`, [ ivm ]);
	strictEqual(result, 1);
}

console.log('pass');

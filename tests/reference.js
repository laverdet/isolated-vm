'use strict';
const ivm = require('isolated-vm');
const { strictEqual, throws } = require('assert');
let trap = false;

{
	// Set up inheritance
	const foo = { foo: 1 };
	const bar = Object.create(foo);
	bar.bar = 2;
	const etc = Object.create(bar);
	etc.etc = bar;

	{
		// Test without inheritance
		const ref = new ivm.Reference(etc);
		strictEqual(ref.getSync('bar'), undefined);
		strictEqual(ref.getSync('etc').getSync('foo'), undefined);
		strictEqual(ref.getSync('etc').getSync('bar'), 2);
		ref.setSync('prop', 1);
		strictEqual(ref.getSync('prop'), 1);
		ref.deleteSync('prop');
		strictEqual(ref.getSync('prop'), undefined);
	}

	{
		// Test with inheritance
		const ref = new ivm.Reference(etc, { unsafeInherit: true });
		strictEqual(ref.getSync('bar'), 2);
		strictEqual(ref.getSync('etc').getSync('foo'), 1);
		strictEqual(ref.getSync('etc').getSync('bar'), 2);
		ref.getSync('etc').setSync('prop', 1);
		ref.setSync('prop', 2);
		strictEqual(ref.getSync('prop'), 2);
		ref.deleteSync('prop');
		strictEqual(ref.getSync('prop'), 1);
	}
}

{
	// Set up getter / setters
	let setter = 0;
	const foo = {
		get str() { trap = true; return 'got' },
		set str(_) { trap = true },
	};
	Object.defineProperty(foo, 0, {
		get() { trap = true; return 'got' },
		set() { trap = true },
	});

	{
		// Test plain accessors
		const ref = new ivm.Reference(foo);
		throws(() => ref.getSync('str'));
		throws(() => ref.getSync(0));
		throws(() => ref.setSync(0, undefined));
	}

	{
		// Test accessors + inheritance
		const ref = new ivm.Reference(Object.create(foo), { unsafeInherit: true });
		throws(() => ref.getSync('str'));
		ref.setSync('str', undefined);
		throws(() => ref.getSync(0));
	}
}

{
	// Set up evil proxy
	const val = { prop: 1 };
	const prox = new Proxy(val, {
		get() { trap = true },
		set() { trap = true },
		getOwnPropertyDescriptor() { trap = true },
		getPrototypeOf() { trap = true },
		has() { trap = true },
		ownKeys() { trap = true },
	});
	const inherited = Object.create(prox);

	{
		// Test proxy
		const ref = new ivm.Reference(prox);
		throws(() => ref.getSync('prop'));
	}

	{
		// Test inherited proxy
		const ref = new ivm.Reference(inherited);
		throws(() => ref.getSync('prop'),);
	}

	{
		// Test deep inherited proxy
		const ref = new ivm.Reference(inherited, { unsafeInherit: true });
		throws(() => ref.getSync('prop'));
	}
}

{
	// Test Array (numeric indices)
	const val = [ 1, 2, 3 ];
	Object.defineProperty(val, 1, { get() { trap = true; return 'got' }});
	const ref = new ivm.Reference(val);
	strictEqual(ref.getSync(0), 1);
	throws(() => ref.getSync(1));
}

{
	// Test ArrayBuffer (numeric indices)
	const val = new Uint8Array([ 1, 2, 3 ]);
	const ref = new ivm.Reference(val);
	strictEqual(ref.getSync(0), 1);
}

if (trap) {
	console.log('fail');
}
console.log('pass');

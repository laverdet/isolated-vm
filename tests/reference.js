const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

const foo = { foo: 1 };
const bar = Object.create(foo);
bar.bar = 2;
const etc = Object.create(bar);
etc.etc = bar;
{
	const ref = new ivm.Reference(etc);
	strictEqual(ref.getSync('bar'), undefined);
	strictEqual(ref.getSync('etc').getSync('foo'), undefined);
	strictEqual(ref.getSync('etc').getSync('bar'), 2);
}
{
	const ref = new ivm.Reference(etc, { inheritUnsafe: true });
	strictEqual(ref.getSync('bar'), 2);
	strictEqual(ref.getSync('etc').getSync('foo'), 1);
	strictEqual(ref.getSync('etc').getSync('bar'), 2);
}

console.log('pass');

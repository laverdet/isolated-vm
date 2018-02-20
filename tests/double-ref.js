'use strict';
let ivm = require('isolated-vm');
let ref = new ivm.Reference('hello');
(new ivm.Reference(function(a, b) {
	a.dispose();
	b.deref();
	console.log('pass');
})).applySync(undefined, [ ref, ref ]);

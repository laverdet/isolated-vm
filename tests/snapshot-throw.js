'use strict';
try {
	require("isolated-vm").Isolate.createSnapshot([ { code: '**' } ]);
} catch (err) {
	console.log('pass');
}

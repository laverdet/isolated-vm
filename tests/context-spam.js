let ivm = require('isolated-vm');

let isolate = new ivm.Isolate({ memoryLimit: 32 });
try {
	while (true) {
		isolate.createContextSync();
	}
} catch (err) {}

(async function () {
	let isolate = new ivm.Isolate({ memoryLimit: 32 });
	while (true) {
		await isolate.createContext();
	}
}()).catch(() => console.log('pass'));

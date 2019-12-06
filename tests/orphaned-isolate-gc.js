// node-args: --expose-gc
const ivm = require('isolated-vm');
let isolate = Array(100).fill().map(() => new ivm.Isolate());
gc();
let rss1 = process.memoryUsage().rss;
isolate = null;
gc();
let rss2 = process.memoryUsage().rss;
if (rss1 - rss2 > 20 * 1024 * 1024) {
	console.log('pass');
} else {
	console.log(`${rss1} - ${rss2} = ${rss1 - rss2}`);
}

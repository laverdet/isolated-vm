const ivm = require('isolated-vm');

const isolate = new ivm.Isolate();
const context = isolate.createContextSync();
context.global.setSync("getProm", () => {}, { reference: true });
context.eval("getProm.applySync()").catch(() => console.log('pass'));
context.release();
process.nextTick(() => {});
isolate.dispose();

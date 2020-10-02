const ivm = require('isolated-vm');
const assert = require('assert');

(async () => {
  const isolate = new ivm.Isolate;
  const context = isolate.createContextSync();
  context.global.set('isolate', isolate);
  context.evalIgnored('const d = Date.now(); while (Date.now() < d + 500); isolate.dispose();');
  await new Promise(resolve => setTimeout(resolve, 250));
  try {
    await isolate.createContext();
  } catch (err) {
    assert.ok(/disposed/.test(err.message));
  }
  console.log('pass');
})().catch(console.log);

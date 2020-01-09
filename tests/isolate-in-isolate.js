const ivm = require('isolated-vm');

const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
context.global.setSync('ivm', ivm);
context.eval(`
  const isolate = new ivm.Isolate;
  isolate.createContext();
`).then(() => console.log('pass'), err => console.error(err));

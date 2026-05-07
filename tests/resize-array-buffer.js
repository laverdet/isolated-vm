const ivm = require('isolated-vm');

const isolate = new ivm.Isolate({ memoryLimit: 128 });
const context = isolate.createContextSync();

context.evalClosureSync(
  `const buffer = new ArrayBuffer(8, { maxByteLength: 16 });
  const view = new Uint8Array(buffer, 4, 4);
  buffer.resize(2);
  return view;
  `,
  [],
  {
    result: { copy: true },
    timeout: 1000,
  },
);
console.log('pass');

'use strict';
let ivm = require('isolated-vm');
let assert = require('assert');

let arr = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ];
const buffer = Buffer.from(arr)

// Without buffer prototype set, buffers copied from isolate are pure Uint8Array instances
{
    const isolate = new ivm.Isolate();
    const context = isolate.createContextSync();
    const jail = context.global;

    jail.setSync('buffer', buffer, { copy: true });

    const isUint8ArrayInSandbox = context.evalClosureSync(`
        return buffer instanceof Uint8Array && buffer.constructor.name === 'Uint8Array'
    `, [], { result: { copy: true } });

    assert(isUint8ArrayInSandbox === true);

    const bufferCopy = jail.getSync('buffer', { copy: true });

    assert(bufferCopy !== buffer);
    assert(bufferCopy instanceof Uint8Array);
    assert(!Buffer.isBuffer(bufferCopy));
}

{
    const isolate = new ivm.Isolate();
    const context = isolate.createContextSync();
    const jail = context.global;

    context.evalClosureSync(`
        class Buffer extends Uint8Array {}
        globalThis.Buffer = Buffer;
        $0.setBufferPrototype(Buffer.prototype);
    `, [isolate]);

    jail.setSync('buffer', buffer, { copy: true });

    const isBufferObjectInSandbox = context.evalClosureSync(`
        return buffer instanceof Buffer && buffer.constructor.name === 'Buffer'
    `, [], { result: { copy: true } });

    assert(isBufferObjectInSandbox === true);

    const bufferCopy = jail.getSync('buffer', { copy: true });

    assert(bufferCopy !== buffer);
    assert(Buffer.isBuffer(bufferCopy));
    assert.deepStrictEqual(bufferCopy, buffer);
}

{
    const isolate = new ivm.Isolate();
    const context = isolate.createContextSync();
    const jail = context.global;

    context.evalClosureSync(`
        class Buffer extends Uint8Array {}
        globalThis.Buffer = Buffer;
        $0.setBufferPrototype(Buffer.prototype);
    `, [isolate]);

    jail.setSync('objectWithBuffer', { buffer, array: [buffer] }, { copy: true });

    const objectWithBuffer = jail.getSync('objectWithBuffer', { copy: true });

    assert(Buffer.isBuffer(objectWithBuffer.buffer));
    assert(Buffer.isBuffer(objectWithBuffer.array[0]));
}

console.log('pass');

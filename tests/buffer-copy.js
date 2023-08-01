'use strict';
let ivm = require('isolated-vm');
let assert = require('assert');

const bufferViews = [
    new DataView(new ArrayBuffer(42)),
    new Int8Array([1, 2, 3]),
    new Int16Array([1, 2, 3]),
    new Int32Array([1, 2, 3]),
    new Uint8Array([1, 2, 3]),
    new Uint8ClampedArray([1, 2, 3]),
    new Uint16Array([1, 2, 3]),
    new Uint32Array([1, 2, 3]),
    new Float32Array([1, 2, 3]),
    new Float64Array([1, 2, 3]),
    new BigInt64Array([1n, 2n, 3n]),
    new BigUint64Array([1n, 2n, 3n]),
];

for (const buffer of bufferViews) {
    const isolate = new ivm.Isolate();

    const context = isolate.createContextSync();
    const jail = context.global;

    context.evalSync(`function identity(param) { return param; }`);

    const identityFunctionRef = jail.getSync('identity', { reference: true });

    // copy without properties
    const resultWithoutProps = identityFunctionRef.applySync(null, [buffer], { result: { copy: true }, arguments: { copy: true } });
    assert.deepStrictEqual(buffer, resultWithoutProps);

    // copy with extra properties
    buffer.stringProp = "string";
    buffer.numberProp = 42;
    buffer.boolProp = true;
    buffer.nullProp = null;
    buffer.undefinedProp = undefined;
    buffer.bigIntProp = 42n;
    buffer.emptyArrayProp = [];
    buffer.arrayProp = ["string", 42, true, null, undefined, 42n, [], {}];
    buffer.emptyObjectProp = {};
    buffer.objectProp = {
        stringProp: "string",
        numberProp: 42,
        boolProp: true,
        nullProp: null,
        undefinedProp: undefined,
        bigIntProp: 42n,
        arrayProp: [],
        nested: {
            prop: "nested"
        }
    }

    const resultWithProps = identityFunctionRef.applySync(null, [buffer], { result: { copy: true }, arguments: { copy: true } });
    assert.deepStrictEqual(buffer, resultWithProps);
}

console.log('pass');
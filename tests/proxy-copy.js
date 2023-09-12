'use strict';
let ivm = require('isolated-vm');
let assert = require('assert');

// Copying Proxy Object directly
{
    const isolate = new ivm.Isolate();
    const context = isolate.createContextSync();
    const jail = context.global;

    const targetObject = { property: 'value' };
    const proxyObject = new Proxy(targetObject, {});

    jail.setSync('proxyObject', proxyObject, { copy: true });

    const isProxyObjectInSandbox = context.evalClosureSync(`
        return proxyObject.property === 'value'
    `, [], { result: { copy: true } });

    assert(isProxyObjectInSandbox === true);

    const proxyObjectCopy = jail.getSync('proxyObject', { copy: true });

    assert(proxyObjectCopy !== proxyObject);
    assert.deepStrictEqual(targetObject, proxyObjectCopy)
}

// Copy nested Proxy Object will fail, unfortunately V8 serializer will throw on Proxy object, and
// it looks like there is no way to enable proxy serialization.
{
    const isolate = new ivm.Isolate();
    const context = isolate.createContextSync();
    const jail = context.global;

    const targetObject = { property: 'value' };
    const proxyObject = new Proxy(targetObject, {});

    const objectWithNesterProxyObject = { proxyObject };

    try {
        jail.setSync('objectWithNesterProxyObject', objectWithNesterProxyObject, { copy: true });
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
}

console.log('pass');

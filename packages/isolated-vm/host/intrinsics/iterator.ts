// %AsyncIteratorPrototype% is only reachable through an async generator
const AsyncGeneratorFunctionPrototype = Object.getPrototypeOf(async function*() {}) as { prototype: object };
/** @internal */ export const AsyncIteratorPrototype = Object.getPrototypeOf(AsyncGeneratorFunctionPrototype.prototype) as object;

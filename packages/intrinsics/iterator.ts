import type { Constructor } from "./utility.js";

export const AsyncGeneratorFunctionPrototype = Object.getPrototypeOf(async function*() {}) as Constructor<AsyncGenerator>;
export const AsyncIteratorPrototype = Object.getPrototypeOf(AsyncGeneratorFunctionPrototype.prototype) as Constructor<AsyncIterator<unknown>>;

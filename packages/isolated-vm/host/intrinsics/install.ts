import { ArrayPrototype, applySafeArray } from "@isolated-vm/experimental/host/intrinsics/array";
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";
import { NumberConstructor, NumberIsSafeInteger } from "@isolated-vm/experimental/host/intrinsics/number";
import { ObjectDefineProperties, ObjectDefineProperty, ObjectGetPrototypeOf, ObjectPrototype, ObjectSetPrototypeOf } from "@isolated-vm/experimental/host/intrinsics/object";
import { ReflectDefineProperty, ReflectGetOwnPropertyDescriptor, ReflectOwnKeys, ReflectSetPrototypeOf } from "@isolated-vm/experimental/host/intrinsics/reflect";
import { StringConstructor } from "@isolated-vm/experimental/host/intrinsics/string";

type DefineGetter = (this: object, name: PropertyKey, value: unknown) => void;
type DefineSetter = (this: object, name: PropertyKey, value: unknown) => void;
// @ts-expect-error
const ObjectDefineGetter = makeFreeFunction(Object.prototype.__defineGetter__ as DefineGetter);
// @ts-expect-error
const ObjectDefineSetter = makeFreeFunction(Object.prototype.__defineSetter__ as DefineSetter);

function set(target: object, key: string, value: (...args: any[]) => unknown) {
	ObjectDefineProperty(value, "name", { value: key, configurable: true });
	ObjectDefineProperty(target, key, { value, writable: true, configurable: true });
}

function coercePropertyKey(key: PropertyKey) {
	return typeof key === "symbol" ? key : `${key}`;
}

function isArrayPrototypeChain(target: unknown) {
	return target === ArrayPrototype || target === ObjectPrototype;
}

function observePropertyKey(key: string | symbol) {
	if (typeof key !== "symbol") {
		const index = NumberConstructor(key);
		if (index >= 0 && NumberIsSafeInteger(index) && StringConstructor(index) === key) {
			applySafeArray();
		}
	}
	return key;
}

function observePrototypeTarget(target: unknown) {
	if (target === ArrayPrototype) {
		applySafeArray();
	}
	return target;
}

// Watched defined properties
set(Object, "defineProperty", (target: object, name: PropertyKey, descriptor: PropertyDescriptor) =>
	ObjectDefineProperty(target, isArrayPrototypeChain(target) ? observePropertyKey(coercePropertyKey(name)) : name, descriptor));

set(Object, "defineProperties", (target: object, descriptors: PropertyDescriptorMap) => {
	if (isArrayPrototypeChain(target)) {
		const keys = ReflectOwnKeys(descriptors);
		const length = keys.length;
		for (let ii = 0; ii < length; ii++) {
			if (ReflectGetOwnPropertyDescriptor(descriptors, keys[ii]!)?.enumerable) {
				const key = observePropertyKey(keys[ii]!);
				ObjectDefineProperty(target, key, descriptors[key]!);
			}
		}
		return target;
	} else {
		return ObjectDefineProperties(target, descriptors);
	}
});

set(Reflect, "defineProperty", (target: object, name: PropertyKey, descriptor: PropertyDescriptor) =>
	ReflectDefineProperty(target, isArrayPrototypeChain(target) ? observePropertyKey(coercePropertyKey(name)) : name, descriptor));

set(Object.prototype, "__defineGetter__", function(this: object, name: PropertyKey, value: unknown) {
	ObjectDefineGetter(this, isArrayPrototypeChain(this) ? observePropertyKey(coercePropertyKey(name)) : name, value);
});

set(Object.prototype, "__defineSetter__", function(this: object, name: PropertyKey, value: unknown) {
	ObjectDefineSetter(this, isArrayPrototypeChain(this) ? observePropertyKey(coercePropertyKey(name)) : name, value);
});

// Watch modified prototypes
set(Object, "setPrototypeOf", (target: object, prototype: object | null) =>
	ObjectSetPrototypeOf(observePrototypeTarget(target), prototype));

set(Reflect, "setPrototypeOf", (target: object, prototype: object | null) =>
	ReflectSetPrototypeOf(observePrototypeTarget(target) as object, prototype));

ObjectDefineProperty(Object.prototype, "__proto__", {
	configurable: true,
	get(this: object): unknown { return ObjectGetPrototypeOf(this); },
	set(this: object, prototype: object | null) {
		if (prototype === null || typeof prototype === "object" || typeof prototype === "function") {
			ObjectSetPrototypeOf(observePrototypeTarget(this), prototype);
		}
	},
});

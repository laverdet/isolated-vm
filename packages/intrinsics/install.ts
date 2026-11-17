import { ArrayPrototype } from "@isolated-vm/intrinsics/array";
import { TypeErrorConstructor } from "@isolated-vm/intrinsics/error";
import { ObjectCreate, ObjectDefineProperties, ObjectDefineProperty, ObjectGetPrototypeOf, ObjectPrototype, ObjectSetPrototypeOf } from "@isolated-vm/intrinsics/object";
import { NumberConstructor, NumberIsSafeInteger, StringConstructor, SymbolSpecies } from "@isolated-vm/intrinsics/primitive";
import { ReflectDefineProperty, ReflectGetOwnPropertyDescriptor, ReflectOwnKeys, ReflectSetPrototypeOf } from "@isolated-vm/intrinsics/reflect";
import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

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

function checkIndexedPropertyKey(key: string | symbol) {
	if (typeof key !== "symbol") {
		const index = NumberConstructor(key);
		if (index >= 0 && NumberIsSafeInteger(index) && StringConstructor(index) === key) {
			throw new TypeErrorConstructor("Cannot define an indexed property on the array prototype chain");
		}
	}
	return key;
}

function checkPrototypeTarget(target: unknown) {
	if (target === ArrayPrototype) {
		throw new TypeErrorConstructor("Cannot set the prototype of Array.prototype");
	}
	return target;
}

// Watch defined properties
set(Object, "defineProperty", (target: object, name: PropertyKey, descriptor: PropertyDescriptor) =>
	ObjectDefineProperty(target, isArrayPrototypeChain(target) ? checkIndexedPropertyKey(coercePropertyKey(name)) : name, descriptor));

set(Object, "defineProperties", (target: object, descriptors: PropertyDescriptorMap) => {
	if (isArrayPrototypeChain(target)) {
		const properties = ObjectCreate(null) as Record<PropertyKey, PropertyDescriptor>;
		const keys = ReflectOwnKeys(descriptors);
		const length = keys.length;
		for (let ii = 0; ii < length; ii++) {
			if (ReflectGetOwnPropertyDescriptor(descriptors, keys[ii]!)?.enumerable) {
				const key = checkIndexedPropertyKey(keys[ii]!);
				properties[key] = descriptors[key]!;
			}
		}
		return ObjectDefineProperties(target, properties);
	} else {
		return ObjectDefineProperties(target, descriptors);
	}
});

set(Reflect, "defineProperty", (target: object, name: PropertyKey, descriptor: PropertyDescriptor) =>
	ReflectDefineProperty(target, isArrayPrototypeChain(target) ? checkIndexedPropertyKey(coercePropertyKey(name)) : name, descriptor));

set(Object.prototype, "__defineGetter__", function(this: object, name: PropertyKey, value: unknown) {
	ObjectDefineGetter(this, isArrayPrototypeChain(this) ? checkIndexedPropertyKey(coercePropertyKey(name)) : name, value);
});

set(Object.prototype, "__defineSetter__", function(this: object, name: PropertyKey, value: unknown) {
	ObjectDefineSetter(this, isArrayPrototypeChain(this) ? checkIndexedPropertyKey(coercePropertyKey(name)) : name, value);
});

// Watch modified prototypes
set(Object, "setPrototypeOf", (target: object, prototype: object | null) =>
	ObjectSetPrototypeOf(checkPrototypeTarget(target), prototype));

set(Reflect, "setPrototypeOf", (target: object, prototype: object | null) =>
	ReflectSetPrototypeOf(checkPrototypeTarget(target) as object, prototype));

ObjectDefineProperty(Object.prototype, "__proto__", {
	configurable: true,
	get(this: object): unknown { return ObjectGetPrototypeOf(this); },
	set(this: object, prototype: object | null) {
		if (prototype === null || typeof prototype === "object" || typeof prototype === "function") {
			ObjectSetPrototypeOf(checkPrototypeTarget(this), prototype);
		}
	},
});

// Freeze crucial species
function freezeSpecies(constructor: { readonly prototype: object }) {
	ObjectDefineProperty(constructor.prototype, "constructor", { writable: false, configurable: false });
	ObjectDefineProperty(constructor, SymbolSpecies, { configurable: false });
}

freezeSpecies(Array);
freezeSpecies(Promise);

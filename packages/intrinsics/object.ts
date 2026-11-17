export const ObjectPrototype = Object.prototype;

export const ObjectCreate = Object.create;
export const ObjectDefineProperties = Object.defineProperties;
export const ObjectDefineProperty = Object.defineProperty;
export const ObjectFreeze = Object.freeze;
export const ObjectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
export const ObjectGetPrototypeOf = Object.getPrototypeOf;
export const ObjectSetPrototypeOf = Object.setPrototypeOf;

export const kEmptyObject = ObjectFreeze(ObjectCreate(null)) as Readonly<Record<PropertyKey, never>>;

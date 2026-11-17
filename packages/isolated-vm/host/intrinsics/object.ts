/** @internal */ export const ObjectPrototype = Object.prototype;

/** @internal */ export const ObjectCreate = Object.create;
/** @internal */ export const ObjectDefineProperties = Object.defineProperties;
/** @internal */ export const ObjectDefineProperty = Object.defineProperty;
/** @internal */ export const ObjectFreeze = Object.freeze;
/** @internal */ export const ObjectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
/** @internal */ export const ObjectGetPrototypeOf = Object.getPrototypeOf;
/** @internal */ export const ObjectSetPrototypeOf = Object.setPrototypeOf;

/** @internal */ export const kEmptyObject = ObjectFreeze(ObjectCreate(null)) as Readonly<Record<PropertyKey, never>>;

/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

// Boolean
export const BooleanConstructor = Boolean;

// Number
export const NumberConstructor = Number;
export const NumberIsFinite = Number.isFinite;
export const NumberIsNaN = Number.isNaN;
export const NumberIsSafeInteger = Number.isSafeInteger;
export const Number_MAX_SAFE_INTEGER = Number.MAX_SAFE_INTEGER;

// String
export const StringConstructor = String;
export const StringCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
export const StringFromCharCode = String.fromCharCode;

// Symbol
export const SymbolAsyncIterator = Symbol.asyncIterator;
export const SymbolIterator = Symbol.iterator;
export const SymbolToStringTag = Symbol.toStringTag;

/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";

/** @internal */ export const StringConstructor = String;
/** @internal */ export const StringCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
/** @internal */ export const StringFromCharCode = String.fromCharCode;

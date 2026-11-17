/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";

/** @internal */ export const WeakRefConstructor = WeakRef;
/** @internal */ export const WeakRefDeref: <Type extends WeakKey>(thisArg: WeakRef<Type>) => Type | undefined = makeFreeFunction(WeakRef.prototype.deref);

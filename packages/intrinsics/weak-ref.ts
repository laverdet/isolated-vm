/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

export const WeakRefConstructor = WeakRef;
export const WeakRefDeref: <Type extends WeakKey>(thisArg: WeakRef<Type>) => Type | undefined = makeFreeFunction(WeakRef.prototype.deref);

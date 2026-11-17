/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";

/** @internal */ export const MapConstructor = Map;
/** @internal */ export const MapDelete: <K, V>(thisArg: Map<K, V>, key: K) => boolean = makeFreeFunction(Map.prototype.delete);
/** @internal */ export const MapGet: <K, V>(thisArg: Map<K, V>, key: K) => V | undefined = makeFreeFunction(Map.prototype.get);
/** @internal */ export const MapSet: <K, V>(thisArg: Map<K, V>, key: K, value: V) => Map<K, V> = makeFreeFunction(Map.prototype.set);

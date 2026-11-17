/* eslint-disable @typescript-eslint/unbound-method */
import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

export const MapConstructor = Map;
export const MapDelete: <K, V>(thisArg: Map<K, V>, key: K) => boolean = makeFreeFunction(Map.prototype.delete);
export const MapGet: <K, V>(thisArg: Map<K, V>, key: K) => V | undefined = makeFreeFunction(Map.prototype.get);
export const MapSet: <K, V>(thisArg: Map<K, V>, key: K, value: V) => Map<K, V> = makeFreeFunction(Map.prototype.set);

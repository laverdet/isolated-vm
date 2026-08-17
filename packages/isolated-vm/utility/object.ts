/** @internal */
export type Constructor<Type> = (abstract new (...args: any[]) => Type);

type AddThis<Type, Fn> =
	Fn extends (...args: infer Args) => infer Return
		? (this: Type, ...args: Args) => Return
		: never;

/** @internal */
export function extend<Type, Proto extends {
	[Key in keyof Type]?: AddThis<Type, Type[Key]>;
}>(constructor: Constructor<Type>, prototype: Proto): void {
	const descriptors = Object.getOwnPropertyDescriptors(prototype);
	for (const key of Reflect.ownKeys(descriptors)) {
		const info = descriptors[key as keyof Proto];
		if (info.value && typeof info.value === "function") {
			Object.defineProperty(constructor.prototype, key, { ...info, enumerable: false });
		} else {
			throw new Error(`Cannot extend ${constructor.name} with non-function property ${String(key)}`);
		}
	}
}

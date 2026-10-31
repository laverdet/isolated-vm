/** @internal */
export interface Constructor<Type> {
	readonly name: string;
	readonly prototype: Type;
}

type AddThis<Type, Fn> =
	Fn extends (...args: infer Args) => infer Return
		? (this: Type, ...args: Args) => Return
		: never;

/** @internal */
export function extend<Type extends object, Properties extends object>(
	constructor: Constructor<Type> & Properties,
	prototype: {
		[Key in keyof Type]?: AddThis<Type, Type[Key]>;
	},
	properties?: {
		[Key in keyof Properties]?: Properties[Key];
	},
): void {
	for (const key of Reflect.ownKeys(prototype)) {
		const descriptor = Object.getOwnPropertyDescriptor(prototype, key);
		if (typeof descriptor?.value !== "function") {
			throw new Error(`Cannot extend ${constructor.name} with non-function property ${String(key)}`);
		}
		Object.defineProperty(constructor.prototype, key, { ...descriptor, enumerable: false });
	}
	if (properties) {
		for (const key of Reflect.ownKeys(properties)) {
			const descriptor = Object.getOwnPropertyDescriptor(properties, key)!;
			Object.defineProperty(constructor, key, descriptor);
		}
	}
}

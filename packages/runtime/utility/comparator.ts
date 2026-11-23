/** @internal */
export type Comparator<Type> = (left: Type, right: Type) => number;

type PrimitiveComparable = bigint | boolean | string;

/**
 * Creates a comparator from a mapping function and a comparator.
 * @internal
 */
export function mappedComparator<Type, Result>(
	comparator: Comparator<Result>,
	map: (value: Type) => Result,
): Comparator<Type> {
	return (left, right) => comparator(map(left), map(right));
}

/**
 * A comparator which can be used mainly for strings, but also bigint / booleans if you feel the
 * need for that kind of thing. You could use it for numbers too, but that's better suited to
 * `numeric` so the types don't permit it in that case.
 * @internal
 */
export function primitiveComparator<Type extends PrimitiveComparable>(left: Type, right: Type): number {
	if (left === right) {
		return 0;
	} else {
		return left < right ? -1 : 1;
	}
}

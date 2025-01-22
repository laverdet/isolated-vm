/** @internal */
export type Comparator<Type> = (left: Type, right: Type) => number;

/** @internal */
export function mappedComparator<Type, Result>(
	comparator: Comparator<Result>,
	map: (value: Type) => Result,
): Comparator<Type> {
	return (left, right) => comparator(map(left), map(right));
}

/** @internal */
export function selectKey<Key, Value>(entry: [Key, Value]): Key {
	return entry[0];
}

/** @internal */
export function stringComparator(left: string, right: string) {
	return left < right ? -1 : left === right ? 0 : 1;
}

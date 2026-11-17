/** @internal */
export type Comparator<Type> = (left: Type, right: Type) => number;

/**
 * Array lower bound bisect
 * @internal
 */
export function arrayLowerBound<Type extends Value, Value>(
	array: readonly Type[],
	value: Value,
	comparator: Comparator<Value>,
): number {
	return bisect(
		0, array.length,
		(left, right) => (left + right) >> 1,
		ii => comparator(array[ii]!, value));
}

/**
 * Generic bisect
 * @internal
 */
export function bisect<Type>(
	first: Type,
	last: Type,
	divide: (low: Type, high: Type) => Type,
	boundComparator: (value: Type) => number,
): Type {
	if (first === last) {
		return first;
	}
	let low = first;
	let high = last;
	while (true) {
		const mid = divide(low, high);
		if (boundComparator(mid) < 0) {
			if (mid === low) {
				return high;
			}
			low = mid;
		} else {
			if (mid === high) {
				return low;
			}
			high = mid;
		}
	}
}

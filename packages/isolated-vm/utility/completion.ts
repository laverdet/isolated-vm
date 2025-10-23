import type { MaybeCompletionOf } from "backend_v8.node";

/** @internal */
export function transformCompletion<From, Type>(
	result: MaybeCompletionOf<From>,
	transform: (value: From) => Type,
): MaybeCompletionOf<Type> {
	if (result?.complete) {
		return { complete: true, result: transform(result.result) };
	} else {
		return result;
	}
}

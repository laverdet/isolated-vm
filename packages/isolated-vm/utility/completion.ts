import * as assert from "node:assert/strict";

export type CompletionOf<Type> = NormalCompletion<Type> | ThrowCompletion;

export type MaybeCompletionOf<Type> = CompletionOf<Type> | undefined;

export interface NormalCompletion<Type> {
	complete: true;
	error?: never;
	result: Type;
}

export interface ThrowCompletion {
	complete: false;
	error: unknown;
	result?: never;
}

export function expectComplete<Type>(completion: MaybeCompletionOf<Type>): Type {
	assert.ok(completion);
	assert.ok(completion.complete);
	return completion.result;
}

export function expectThrow(completion: MaybeCompletionOf<unknown>): unknown {
	assert.ok(completion);
	assert.ok(!completion.complete);
	return completion.error;
}

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

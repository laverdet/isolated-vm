import * as assert from "node:assert/strict";

export type CompletionOf<Type> = NormalCompletion<Type> | ThrowCompletion;

export type MaybeCompletionOf<Type> = CompletionOf<Type> | null;

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

export function expect<Type>(completion: Type | null): Type {
	assert.ok(completion !== null);
	return completion;
}

export function expectComplete<Type>(completion: MaybeCompletionOf<Type>): Type {
	assert.ok(completion);
	if (completion.complete) {
		return completion.result;
	} else {
		throw completion.error;
	}
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

// https://dom.spec.whatwg.org/#aborting-ongoing-activities
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { EventHandler } from "@isolated-vm/experimental/host/dom/event/handler";
import { defineAbortSignalBrand } from "@isolated-vm/experimental/host/dom/abort/brand";
import { makeEventHandler, setEventHandler } from "@isolated-vm/experimental/host/dom/event/handler";
import { EventTarget, fireEvent } from "@isolated-vm/experimental/host/dom/event/target";
import { DOMException } from "@isolated-vm/experimental/host/html/exception";
import { setTimeout } from "@isolated-vm/experimental/host/html/timers";
import { ArrayPush, TypeErrorConstructor, WeakRefConstructor, WeakRefDeref } from "@isolated-vm/experimental/host/intrinsics";
import { convertEnforceRangeUnsignedLongLong, defineInterface, requireArguments } from "@isolated-vm/experimental/host/webidl";

/**
 * Internal state of an `AbortSignal`.
 * https://dom.spec.whatwg.org/#interface-AbortSignal
 *
 * `reason` is `undefined` until the signal is aborted. Source and dependent signals are held weakly
 * so that an abandoned `AbortSignal.any()` result, or a source whose controller is gone, can be
 * collected.
 */
interface AbortSignalInternal {
	reason: unknown;
	algorithms: (() => void)[];
	dependent: boolean;
	sourceSignals: WeakRef<AbortSignal>[];
	dependentSignals: WeakRef<AbortSignal>[];
	readonly onabort: EventHandler;
}

// Private state accessors, assigned from within the class body
let constructing = false;
let getAbortSignalInternal: (signal: AbortSignal) => AbortSignalInternal;
let isAbortSignal: (value: unknown) => value is AbortSignal;

/** `AbortSignal` has no public constructor; this is the only way to make one. */
function createAbortSignal() {
	try {
		constructing = true;
		return new AbortSignal();
	} finally {
		constructing = false;
	}
}

/** `reason` defaults to an "AbortError" when not given. */
function coerceAbortReason(reason: unknown) {
	return reason === undefined ? new DOMException("signal is aborted without reason", "AbortError") : reason;
}

/** Appends a weak reference after dropping entries whose targets have been collected. */
function pushWeak(refs: WeakRef<AbortSignal>[], signal: AbortSignal) {
	let length = 0;
	for (let ii = 0; ii < refs.length; ++ii) {
		const ref = refs[ii]!;
		if (WeakRefDeref(ref) !== undefined) {
			refs[length++] = ref;
		}
	}
	refs.length = length;
	ArrayPush(refs, new WeakRefConstructor(signal));
}

// https://dom.spec.whatwg.org/#run-the-abort-steps
function runAbortSteps(signal: AbortSignal, internal: AbortSignalInternal) {
	const { algorithms } = internal;
	internal.algorithms = [];
	for (let ii = 0; ii < algorithms.length; ++ii) {
		algorithms[ii]!();
	}
	fireEvent(signal, "abort");
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
function signalAbort(signal: AbortSignal, reason: unknown) {
	const internal = getAbortSignalInternal(signal);
	if (internal.reason !== undefined) {
		return;
	}
	internal.reason = coerceAbortReason(reason);
	// Every dependent takes the reason before any abort steps run, so observers see a consistent state
	const dependents: AbortSignal[] = [];
	const refs = internal.dependentSignals;
	internal.dependentSignals = [];
	for (let ii = 0; ii < refs.length; ++ii) {
		const dependent = WeakRefDeref(refs[ii]!);
		if (dependent !== undefined) {
			const dependentInternal = getAbortSignalInternal(dependent);
			if (dependentInternal.reason === undefined) {
				dependentInternal.reason = internal.reason;
				ArrayPush(dependents, dependent);
			}
		}
	}
	runAbortSteps(signal, internal);
	for (let ii = 0; ii < dependents.length; ++ii) {
		const dependent = dependents[ii]!;
		runAbortSteps(dependent, getAbortSignalInternal(dependent));
	}
}

/** Links `dependent` to a non-dependent `source` in both directions. */
function linkSignals(dependent: AbortSignal, dependentInternal: AbortSignalInternal, source: AbortSignal) {
	pushWeak(dependentInternal.sourceSignals, source);
	pushWeak(getAbortSignalInternal(source).dependentSignals, dependent);
}

// https://dom.spec.whatwg.org/#create-a-dependent-abort-signal
function createDependentAbortSignal(signals: AbortSignal[]) {
	const result = createAbortSignal();
	const resultInternal = getAbortSignalInternal(result);
	for (let ii = 0; ii < signals.length; ++ii) {
		const { reason } = getAbortSignalInternal(signals[ii]!);
		if (reason !== undefined) {
			resultInternal.reason = reason;
			return result;
		}
	}
	resultInternal.dependent = true;
	// Dependents link to the ultimate sources, so chains of `any()` stay one level deep
	for (let ii = 0; ii < signals.length; ++ii) {
		const signal = signals[ii]!;
		const internal = getAbortSignalInternal(signal);
		if (internal.dependent) {
			const refs = internal.sourceSignals;
			for (let jj = 0; jj < refs.length; ++jj) {
				const source = WeakRefDeref(refs[jj]!);
				if (source !== undefined) {
					linkSignals(result, resultInternal, source);
				}
			}
		} else {
			linkSignals(result, resultInternal, signal);
		}
	}
	return result;
}

// https://dom.spec.whatwg.org/#interface-abortcontroller
export class AbortController {
	readonly #signal: AbortSignal;

	static {
		defineInterface(this);
	}

	constructor() {
		this.#signal = createAbortSignal();
	}

	get signal(): AbortSignal { return this.#signal; }

	// https://dom.spec.whatwg.org/#dom-abortcontroller-abort
	abort(reason: unknown = undefined): void {
		signalAbort(this.#signal, reason);
	}
}

// https://dom.spec.whatwg.org/#interface-AbortSignal
export class AbortSignal extends EventTarget {
	readonly #internal: AbortSignalInternal;

	static {
		defineInterface(this);
		getAbortSignalInternal = signal => signal.#internal;
		isAbortSignal = (value: unknown): value is AbortSignal => {
			try {
				// @ts-expect-error
				return #internal in value;
			} catch {
				return false;
			}
		};
		defineAbortSignalBrand({
			isAbortSignal,
			isAborted: signal => signal.#internal.reason !== undefined,
			// https://dom.spec.whatwg.org/#abortsignal-add
			addAbortAlgorithm: (signal, algorithm) => {
				const internal = signal.#internal;
				if (internal.reason === undefined) {
					ArrayPush(internal.algorithms, algorithm);
				}
			},
		});
	}

	constructor() {
		if (!constructing) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		super();
		this.#internal = {
			reason: undefined,
			algorithms: [],
			dependent: false,
			sourceSignals: [],
			dependentSignals: [],
			onabort: makeEventHandler(),
		};
	}

	// https://dom.spec.whatwg.org/#dom-abortsignal-abort
	static abort(reason: unknown = undefined): AbortSignal {
		const signal = createAbortSignal();
		signal.#internal.reason = coerceAbortReason(reason);
		return signal;
	}

	// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
	/** Requires the timers capability to have been initialized in this realm. */
	static timeout(milliseconds: unknown): AbortSignal {
		requireArguments(arguments.length, 1);
		const delay = convertEnforceRangeUnsignedLongLong(milliseconds);
		const signal = createAbortSignal();
		// The closure keeps the signal alive while the timer is pending
		setTimeout(() => signalAbort(signal, new DOMException("signal timed out", "TimeoutError")), delay);
		return signal;
	}

	// https://dom.spec.whatwg.org/#dom-abortsignal-any
	static any(signals: unknown): AbortSignal {
		requireArguments(arguments.length, 1);
		// https://webidl.spec.whatwg.org/#es-sequence -- strings are iterable but are not sequences
		if (signals == null || (typeof signals !== "object" && typeof signals !== "function")) {
			throw new TypeErrorConstructor("Provided value is not of type 'sequence'");
		}
		const list: AbortSignal[] = [];
		// Sequence conversion deliberately iterates the developer's object
		for (const signal of signals as Iterable<unknown>) {
			if (!isAbortSignal(signal)) {
				throw new TypeErrorConstructor("Provided value is not of type 'AbortSignal'");
			}
			ArrayPush(list, signal);
		}
		return createDependentAbortSignal(list);
	}

	get aborted(): boolean { return this.#internal.reason !== undefined; }
	get reason(): unknown { return this.#internal.reason; }

	get onabort(): object | null { return this.#internal.onabort.value; }
	set onabort(value: unknown) { setEventHandler(this.#internal.onabort, this, "abort", value); }

	// https://dom.spec.whatwg.org/#dom-abortsignal-throwifaborted
	throwIfAborted(): void {
		const { reason } = this.#internal;
		if (reason !== undefined) {
			// eslint-disable-next-line @typescript-eslint/only-throw-error -- the reason is any developer-supplied value
			throw reason;
		}
	}
}

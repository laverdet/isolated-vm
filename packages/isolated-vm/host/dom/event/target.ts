// https://dom.spec.whatwg.org/#interface-eventtarget
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
/* eslint-disable @typescript-eslint/restrict-template-expressions -- `${value}` is `ToString`, which is the DOMString conversion */
import type { AbortSignal } from "@isolated-vm/experimental/host/dom/abort";
import type { EventInternal } from "@isolated-vm/experimental/host/dom/event/event";
import type { Dictionary } from "@isolated-vm/experimental/host/webidl";
import { addAbortAlgorithm, isAbortSignal, isAborted } from "@isolated-vm/experimental/host/dom/abort/brand";
import { Event, EventPhases, getEventInternal, isEvent } from "@isolated-vm/experimental/host/dom/event/event";
import { DOMException, reportException } from "@isolated-vm/experimental/host/html/exception";
import { ArrayIndexOf, ArrayPush, ArraySlice, ArraySplice, BooleanConstructor, ReflectApply, TypeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";
import { defineInterface, requireArguments } from "@isolated-vm/experimental/host/webidl";

// https://dom.spec.whatwg.org/#concept-event-listener
interface EventListenerRecord {
	readonly type: string;
	readonly callback: object;
	readonly capture: boolean;
	readonly passive: boolean;
	readonly once: boolean;
	removed: boolean;
}

// Private state accessor, assigned from within the class body
let getEventListenerList: (target: EventTarget) => EventListenerRecord[];

// https://webidl.spec.whatwg.org/#es-callback-interface
/** Converts a nullable `EventListener?` callback interface value. */
function convertEventListener(value: unknown): object | null {
	if (value == null) {
		return null;
	} else if (typeof value === "object" || typeof value === "function") {
		return value;
	}
	throw new TypeErrorConstructor("Provided value is not of type 'EventListener'");
}

// https://dom.spec.whatwg.org/#event-flatten-more
/**
 * Converts `(AddEventListenerOptions or boolean)`. Objects are the dictionary; `undefined`, `null`,
 * and every other primitive fall through to `ToBoolean` as the capture flag.
 */
function flattenOptions(options: unknown) {
	if (options != null && (typeof options === "object" || typeof options === "function")) {
		const { capture, once, passive, signal } = options as Dictionary;
		return {
			capture: BooleanConstructor(capture),
			once: BooleanConstructor(once),
			passive: BooleanConstructor(passive),
			signal,
		};
	}
	return {
		capture: BooleanConstructor(options),
		once: false,
		passive: false,
		signal: undefined,
	};
}

// https://dom.spec.whatwg.org/#remove-an-event-listener
function removeAnEventListener(listeners: EventListenerRecord[], listener: EventListenerRecord) {
	listener.removed = true;
	const index = ArrayIndexOf(listeners, listener);
	if (index !== -1) {
		ArraySplice(listeners, index, 1);
	}
}

/** Removes the listener matching `type`, `callback`, and `capture`, if there is one. */
function removeMatchingEventListener(listeners: EventListenerRecord[], type: string, callback: object | null, capture: boolean) {
	for (let ii = 0; ii < listeners.length; ++ii) {
		const listener = listeners[ii]!;
		if (listener.type === type && listener.callback === callback && listener.capture === capture) {
			removeAnEventListener(listeners, listener);
			return;
		}
	}
}

/**
 * Registers a non-capturing listener without argument conversion, for internal consumers such as
 * event handler IDL attributes.
 * @internal
 */
export function addInternalEventListener(target: EventTarget, type: string, callback: object): void {
	ArrayPush(getEventListenerList(target), { type, callback, capture: false, passive: false, once: false, removed: false });
}

/** @internal */
export function removeInternalEventListener(target: EventTarget, type: string, callback: object): void {
	removeMatchingEventListener(getEventListenerList(target), type, callback, false);
}

// https://webidl.spec.whatwg.org/#call-a-user-objects-operation
/** Functions are called with `currentTarget` as `this`; other objects have `handleEvent` looked up at call time. */
function callUserObjectOperation(callback: object, thisArg: EventTarget | null, event: Event) {
	if (typeof callback === "function") {
		ReflectApply(callback, thisArg, [ event ]);
	} else {
		const { handleEvent } = callback as { handleEvent?: unknown };
		if (typeof handleEvent !== "function") {
			throw new TypeErrorConstructor("'handleEvent' is not a function");
		}
		ReflectApply(handleEvent, callback, [ event ]);
	}
}

// https://dom.spec.whatwg.org/#concept-event-listener-invoke
/** Runs one phase's listeners on `target`. The list is snapshotted so listeners added mid-dispatch do not run. */
function invoke(event: Event, internal: EventInternal, target: EventTarget, capture: boolean) {
	if (internal.stopPropagation) {
		return;
	}
	internal.currentTarget = target;
	const listeners = getEventListenerList(target);
	const snapshot = ArraySlice(listeners);
	for (let ii = 0; ii < snapshot.length; ++ii) {
		const listener = snapshot[ii]!;
		if (listener.removed || listener.type !== internal.type || listener.capture !== capture) {
			continue;
		}
		if (listener.once) {
			removeAnEventListener(listeners, listener);
		}
		internal.inPassiveListener = listener.passive;
		try {
			callUserObjectOperation(listener.callback, target, event);
		} catch (error) {
			reportException(error);
		}
		internal.inPassiveListener = false;
		if (internal.stopImmediatePropagation) {
			break;
		}
	}
}

// https://dom.spec.whatwg.org/#concept-event-dispatch
/** With a single-target path both the capturing and bubbling passes run at the target. */
function dispatch(event: Event, internal: EventInternal, target: EventTarget) {
	internal.dispatch = true;
	internal.target = target;
	internal.eventPhase = EventPhases.AT_TARGET;
	invoke(event, internal, target, true);
	invoke(event, internal, target, false);
	internal.eventPhase = EventPhases.NONE;
	internal.currentTarget = null;
	internal.dispatch = false;
	internal.stopPropagation = false;
	internal.stopImmediatePropagation = false;
	return !internal.canceled;
}

// https://dom.spec.whatwg.org/#concept-event-fire
/** Dispatches a new trusted event of `type` at `target`. */
/** @internal */
export function fireEvent(target: EventTarget, type: string): boolean {
	const event = new Event(type);
	const internal = getEventInternal(event);
	internal.isTrusted = true;
	return dispatch(event, internal, target);
}

// https://dom.spec.whatwg.org/#interface-eventtarget
export class EventTarget {
	readonly #listeners: EventListenerRecord[] = [];

	static {
		defineInterface(this);
		getEventListenerList = target => target.#listeners;
	}

	// https://dom.spec.whatwg.org/#dom-eventtarget-addeventlistener
	addEventListener(type: unknown, callback: unknown, options: unknown = {}): void {
		const listeners = this.#listeners;
		requireArguments(arguments.length, 2);
		const typeString = `${type}`;
		const callbackObject = convertEventListener(callback);
		const { capture, once, passive, signal } = flattenOptions(options);
		let abortSignal: AbortSignal | undefined;
		if (signal !== undefined) {
			if (!isAbortSignal(signal)) {
				throw new TypeErrorConstructor("Provided value is not of type 'AbortSignal'");
			} else if (isAborted(signal)) {
				return;
			}
			abortSignal = signal;
		}
		if (callbackObject === null) {
			return;
		}
		// https://dom.spec.whatwg.org/#add-an-event-listener -- duplicates by type, callback, and capture are ignored
		for (let ii = 0; ii < listeners.length; ++ii) {
			const existing = listeners[ii]!;
			if (existing.type === typeString && existing.callback === callbackObject && existing.capture === capture) {
				return;
			}
		}
		const listener: EventListenerRecord = { type: typeString, callback: callbackObject, capture, passive, once, removed: false };
		ArrayPush(listeners, listener);
		// The spec never unregisters this, so the signal keeps the target alive until it aborts
		if (abortSignal !== undefined) {
			addAbortAlgorithm(abortSignal, () => removeAnEventListener(listeners, listener));
		}
	}

	// https://dom.spec.whatwg.org/#dom-eventtarget-removeeventlistener
	removeEventListener(type: unknown, callback: unknown, options: unknown = {}): void {
		const listeners = this.#listeners;
		requireArguments(arguments.length, 2);
		const typeString = `${type}`;
		const callbackObject = convertEventListener(callback);
		const { capture } = flattenOptions(options);
		removeMatchingEventListener(listeners, typeString, callbackObject, capture);
	}

	// https://dom.spec.whatwg.org/#dom-eventtarget-dispatchevent
	dispatchEvent(event: unknown): boolean {
		// Brand check happens before argument conversion
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#listeners;
		requireArguments(arguments.length, 1);
		if (!isEvent(event)) {
			throw new TypeErrorConstructor("Provided value is not of type 'Event'");
		}
		const internal = getEventInternal(event);
		if (internal.dispatch) {
			throw new DOMException("Event is already being dispatched", "InvalidStateError");
		} else if (!internal.initialized) {
			throw new DOMException("Event is uninitialized", "InvalidStateError");
		}
		internal.isTrusted = false;
		return dispatch(event, internal, this);
	}
}

// https://dom.spec.whatwg.org/#events
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
/* eslint-disable @typescript-eslint/restrict-template-expressions -- `${value}` is `ToString`, which is the DOMString conversion */
import type { Dictionary } from "@isolated-vm/experimental/host/webidl";
import { DOMException, reportException } from "@isolated-vm/experimental/host/html/exception";
import { ArrayIndexOf, ArrayPush, ArraySlice, ArraySplice, BooleanConstructor, DateNow, ObjectDefineProperty, ObjectGetOwnPropertyDescriptor, ReflectApply, TypeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";
import { assertDictionary, defineConstants, defineInterface, requireArguments } from "@isolated-vm/experimental/host/webidl";

// https://dom.spec.whatwg.org/#dom-event-none
const NONE = 0;
const CAPTURING_PHASE = 1;
const AT_TARGET = 2;
const BUBBLING_PHASE = 3;
type EventPhase = typeof NONE | typeof CAPTURING_PHASE | typeof AT_TARGET | typeof BUBBLING_PHASE;

/**
 * Internal state of an `Event`.
 * https://dom.spec.whatwg.org/#interface-event
 *
 * Omitted: `path`, `relatedTarget`, `targetTouches`, etc(?)
 */
interface EventInternal {
	type: string;
	target: EventTarget | null;
	currentTarget: EventTarget | null;
	eventPhase: EventPhase;
	bubbles: boolean;
	cancelable: boolean;
	composed: boolean;
	isTrusted: boolean;
	readonly timeStamp: number;
	// flags
	stopPropagation: boolean;
	stopImmediatePropagation: boolean;
	canceled: boolean;
	inPassiveListener: boolean;
	initialized: boolean;
	dispatch: boolean;
}

// https://dom.spec.whatwg.org/#concept-event-listener
interface EventListenerRecord {
	readonly type: string;
	readonly callback: object;
	readonly capture: boolean;
	readonly passive: boolean;
	readonly once: boolean;
	removed: boolean;
}

// Private state accessors, assigned from within the class bodies
let getEventInternal: (event: Event) => EventInternal;
let isEvent: (value: unknown) => value is Event;
let isTrustedDescriptor: PropertyDescriptor;
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

// https://dom.spec.whatwg.org/#concept-event-initialize
function initialize(internal: EventInternal, type: string, bubbles: boolean, cancelable: boolean) {
	internal.initialized = true;
	internal.stopPropagation = false;
	internal.stopImmediatePropagation = false;
	internal.canceled = false;
	internal.isTrusted = false;
	internal.target = null;
	internal.type = type;
	internal.bubbles = bubbles;
	internal.cancelable = cancelable;
}

// https://dom.spec.whatwg.org/#set-the-canceled-flag
function setTheCanceledFlag(internal: EventInternal) {
	if (internal.cancelable && !internal.inPassiveListener) {
		internal.canceled = true;
	}
}

// https://dom.spec.whatwg.org/#remove-an-event-listener
function removeAnEventListener(listeners: EventListenerRecord[], listener: EventListenerRecord) {
	listener.removed = true;
	const index = ArrayIndexOf(listeners, listener);
	if (index !== -1) {
		ArraySplice(listeners, index, 1);
	}
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
	internal.eventPhase = AT_TARGET;
	invoke(event, internal, target, true);
	invoke(event, internal, target, false);
	internal.eventPhase = NONE;
	internal.currentTarget = null;
	internal.dispatch = false;
	internal.stopPropagation = false;
	internal.stopImmediatePropagation = false;
	return !internal.canceled;
}

// https://dom.spec.whatwg.org/#interface-event
export class Event {
	declare static readonly NONE: 0;
	declare static readonly CAPTURING_PHASE: 1;
	declare static readonly AT_TARGET: 2;
	declare static readonly BUBBLING_PHASE: 3;
	declare readonly NONE: 0;
	declare readonly CAPTURING_PHASE: 1;
	declare readonly AT_TARGET: 2;
	declare readonly BUBBLING_PHASE: 3;
	/** `[LegacyUnforgeable]`: an own, non-configurable accessor defined on each instance by the constructor */
	declare readonly isTrusted: boolean;
	readonly #internal: EventInternal;

	static {
		defineInterface(this);
		defineConstants(this, { NONE, CAPTURING_PHASE, AT_TARGET, BUBBLING_PHASE });
	}

	static {
		getEventInternal = event => event.#internal;
		isEvent = (value: unknown): value is Event => {
			if ((typeof value === "object" && value !== null) || typeof value === "function") {
				return #internal in value;
			}
			return false;
		};
		// PropertyDescriptor applied to each instance
		const holder = {
			get isTrusted() { return (this as Event).#internal.isTrusted; },
		};
		isTrustedDescriptor = { ...ObjectGetOwnPropertyDescriptor(holder, "isTrusted"), configurable: false };
	}

	// https://dom.spec.whatwg.org/#dom-event-event
	constructor(type: unknown, eventInitDict: unknown = {}) {
		requireArguments(arguments.length, 1);
		const typeString = `${type}`;
		assertDictionary(eventInitDict);
		const { bubbles, cancelable, composed } = eventInitDict ?? {};
		this.#internal = {
			type: typeString,
			target: null,
			currentTarget: null,
			eventPhase: NONE,
			bubbles: BooleanConstructor(bubbles),
			cancelable: BooleanConstructor(cancelable),
			composed: BooleanConstructor(composed),
			isTrusted: false,
			// TODO: clock
			timeStamp: DateNow(),
			stopPropagation: false,
			stopImmediatePropagation: false,
			canceled: false,
			inPassiveListener: false,
			initialized: true,
			dispatch: false,
		};
		ObjectDefineProperty(this, "isTrusted", isTrustedDescriptor);
	}

	get type(): string { return this.#internal.type; }
	get target(): EventTarget | null { return this.#internal.target; }
	get currentTarget(): EventTarget | null { return this.#internal.currentTarget; }
	get eventPhase(): number { return this.#internal.eventPhase; }
	get bubbles(): boolean { return this.#internal.bubbles; }
	get cancelable(): boolean { return this.#internal.cancelable; }
	get defaultPrevented(): boolean { return this.#internal.canceled; }
	get composed(): boolean { return this.#internal.composed; }
	get timeStamp(): number { return this.#internal.timeStamp; }

	/** @deprecated */
	set cancelBubble(value: unknown) { this.#internal.stopPropagation ||= BooleanConstructor(value); }
	get cancelBubble(): boolean { return this.#internal.stopPropagation; }

	/** @deprecated */
	get returnValue(): boolean { return !this.#internal.canceled; }
	set returnValue(value: unknown) { BooleanConstructor(value) || setTheCanceledFlag(this.#internal); }

	/** @deprecated */
	get srcElement(): EventTarget | null { return this.#internal.target; }

	// https://dom.spec.whatwg.org/#dom-event-composedpath
	composedPath(): EventTarget[] {
		const { dispatch, target } = this.#internal;
		return dispatch && target !== null ? [ target ] : [];
	}

	// https://dom.spec.whatwg.org/#dom-event-stoppropagation
	stopPropagation(): void {
		this.#internal.stopPropagation = true;
	}

	// https://dom.spec.whatwg.org/#dom-event-preventdefault
	preventDefault(): void {
		setTheCanceledFlag(this.#internal);
	}

	// https://dom.spec.whatwg.org/#dom-event-stopimmediatepropagation
	stopImmediatePropagation(): void {
		const internal = this.#internal;
		internal.stopPropagation = true;
		internal.stopImmediatePropagation = true;
	}

	// https://dom.spec.whatwg.org/#dom-event-initevent
	/** @deprecated */
	initEvent(type: unknown, bubbles: unknown = false, cancelable: unknown = false): void {
		const internal = this.#internal;
		requireArguments(arguments.length, 1);
		const typeString = `${type}`;
		if (!internal.dispatch) {
			initialize(internal, typeString, BooleanConstructor(bubbles), BooleanConstructor(cancelable));
		}
	}
}

// https://dom.spec.whatwg.org/#interface-customevent
export class CustomEvent extends Event {
	#detail: unknown;

	static {
		defineInterface(this);
	}

	// https://dom.spec.whatwg.org/#dom-customevent-customevent
	constructor(type: unknown, eventInitDict: unknown = {}) {
		requireArguments(arguments.length, 1);
		super(type, eventInitDict);
		assertDictionary(eventInitDict);
		this.#detail = eventInitDict?.detail ?? null;
	}

	get detail(): unknown {
		return this.#detail;
	}

	// https://dom.spec.whatwg.org/#dom-customevent-initcustomevent
	/** @deprecated */
	initCustomEvent(type: unknown, bubbles: unknown = false, cancelable: unknown = false, detail: unknown = null): void {
		// Brand check happens before argument conversion
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#detail;
		requireArguments(arguments.length, 1);
		const typeString = `${type}`;
		const internal = getEventInternal(this);
		if (!internal.dispatch) {
			initialize(internal, typeString, BooleanConstructor(bubbles), BooleanConstructor(cancelable));
			this.#detail = detail;
		}
	}
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
		// `AbortSignal` is not implemented in this environment, so no value can satisfy its interface type
		if (signal !== undefined) {
			throw new TypeErrorConstructor("Provided value is not of type 'AbortSignal'");
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
		ArrayPush(listeners, { type: typeString, callback: callbackObject, capture, passive, once, removed: false });
	}

	// https://dom.spec.whatwg.org/#dom-eventtarget-removeeventlistener
	removeEventListener(type: unknown, callback: unknown, options: unknown = {}): void {
		const listeners = this.#listeners;
		requireArguments(arguments.length, 2);
		const typeString = `${type}`;
		const callbackObject = convertEventListener(callback);
		const { capture } = flattenOptions(options);
		for (let ii = 0; ii < listeners.length; ++ii) {
			const listener = listeners[ii]!;
			if (listener.type === typeString && listener.callback === callbackObject && listener.capture === capture) {
				removeAnEventListener(listeners, listener);
				return;
			}
		}
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

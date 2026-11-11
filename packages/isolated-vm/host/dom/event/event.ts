// https://dom.spec.whatwg.org/#interface-event
/* eslint-disable @typescript-eslint/restrict-template-expressions -- `${value}` is `ToString`, which is the DOMString conversion */
import type { EventTarget } from "@isolated-vm/experimental/host/dom/event/target";
import { BooleanConstructor, DateNow, ObjectDefineProperty, ObjectFreeze, ObjectGetOwnPropertyDescriptor } from "@isolated-vm/experimental/host/intrinsics";
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
 * @internal
 */
export interface EventInternal {
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

// Private state accessors, assigned from within the class body
/** @internal */ export let getEventInternal: (event: Event) => EventInternal;
/** @internal */ export let isEvent: (value: unknown) => value is Event;
let isTrustedDescriptor: PropertyDescriptor;

/** @internal */
export const EventPhases = ObjectFreeze({ NONE, CAPTURING_PHASE, AT_TARGET, BUBBLING_PHASE } as const);

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

/** @internal */
export function cancelEvent(event: Event): void {
	setTheCanceledFlag(getEventInternal(event));
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
		defineConstants(this, EventPhases);
	}

	static {
		getEventInternal = event => event.#internal;
		isEvent = (value: unknown): value is Event => {
			try {
				// @ts-expect-error
				return #internal in value;
			} catch {
				return false;
			}
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

// https://html.spec.whatwg.org/multipage/webappapis.html#event-handler-attributes
import type { Event } from "@isolated-vm/experimental/host/dom/event/event";
import type { EventTarget } from "@isolated-vm/experimental/host/dom/event/target";
import { cancelEvent } from "@isolated-vm/experimental/host/dom/event/event";
import { addInternalEventListener, removeInternalEventListener } from "@isolated-vm/experimental/host/dom/event/target";
import { ReflectApply } from "@isolated-vm/experimental/host/intrinsics";

/** State of one event handler IDL attribute, such as `onabort`, on one target. */
export interface EventHandler {
	/** The developer-supplied callback, or null */
	value: object | null;
	/** The listener registered on the target while `value` is non-null */
	listener: object | null;
}

export function makeEventHandler(): EventHandler {
	return { value: null, listener: null };
}

// https://html.spec.whatwg.org/multipage/webappapis.html#event-handler-idl-attributes
/**
 * Setter steps for an event handler IDL attribute. The handler's listener is registered the first
 * time a non-null value is set, so it keeps its position in the listener list across reassignment,
 * and removed when set back to null.
 */
export function setEventHandler(handler: EventHandler, target: EventTarget, type: string, value: unknown): void {
	// `[LegacyTreatNonObjectAsNull]`
	const callback = (typeof value === "object" && value !== null) || typeof value === "function" ? value : null;
	handler.value = callback;
	if (callback === null) {
		if (handler.listener !== null) {
			removeInternalEventListener(target, type, handler.listener);
			handler.listener = null;
		}
	} else if (handler.listener === null) {
		const listener = (event: Event) => processEventHandler(handler, target, event);
		handler.listener = listener;
		addInternalEventListener(target, type, listener);
	}
}

// https://html.spec.whatwg.org/multipage/webappapis.html#the-event-handler-processing-algorithm
/** Exceptions propagate to the dispatch logic, which reports them. */
function processEventHandler(handler: EventHandler, target: EventTarget, event: Event) {
	const callback = handler.value;
	// A `[LegacyTreatNonObjectAsNull]` callback which is not callable is invoked as a no-op
	if (typeof callback !== "function") {
		return;
	}
	const returnValue: unknown = ReflectApply(callback, target, [ event ]);
	if (returnValue === false) {
		cancelEvent(event);
	}
}

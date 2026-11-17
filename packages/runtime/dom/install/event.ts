import { CustomEvent, Event, EventTarget } from "@isolated-vm/runtime/dom/event";
import { exposeInterface } from "@isolated-vm/runtime/webidl";

exposeInterface(EventTarget);
exposeInterface(Event);
exposeInterface(CustomEvent);

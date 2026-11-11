import { CustomEvent, Event, EventTarget } from "@isolated-vm/experimental/host/dom/event";
import { exposeInterface } from "@isolated-vm/experimental/host/webidl";

exposeInterface(EventTarget);
exposeInterface(Event);
exposeInterface(CustomEvent);

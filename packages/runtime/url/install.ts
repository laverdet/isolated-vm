import { URL, URLSearchParams } from "@isolated-vm/runtime/url";
import { exposeInterface } from "@isolated-vm/runtime/webidl";

exposeInterface(URL);
exposeInterface(URLSearchParams);

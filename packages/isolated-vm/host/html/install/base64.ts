import { atob, btoa } from "@isolated-vm/experimental/host/html/base64";

globalThis.atob = atob;
globalThis.btoa = btoa;

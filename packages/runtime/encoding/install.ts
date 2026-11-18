import { TextDecoder, TextDecoderStream, TextEncoder, TextEncoderStream } from "@isolated-vm/runtime/encoding";
import { exposeInterface } from "@isolated-vm/runtime/webidl";

exposeInterface(TextDecoder);
exposeInterface(TextDecoderStream);
exposeInterface(TextEncoder);
exposeInterface(TextEncoderStream);

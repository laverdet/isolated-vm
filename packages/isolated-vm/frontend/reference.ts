import type * as backend from "#backend_v8";
import type { MaybeCompletionOf } from "@isolated-vm/experimental";

type InvokeArguments<Value> = Value extends (...args: infer Args) => unknown ? Args : never;
type InvokeResult<Value> = Value extends (...args: unknown[]) => infer Result ? Result : never;

export declare interface Reference<Type = unknown> extends backend.Reference {
	copy: () => Promise<Type>;
	invoke: (args: InvokeArguments<Type>) => Promise<MaybeCompletionOf<InvokeResult<Type>>>;
	set: (property: string, value: unknown) => Promise<void>;

	// eslint-disable-next-line @typescript-eslint/method-signature-style
	get<Key extends keyof Type>(property: Key): Promise<Reference<Type[Key]>>;
	// eslint-disable-next-line @typescript-eslint/method-signature-style
	get(property: string): Promise<Reference>;
}

export module v8_js:evaluation.script;
import :accept;
import :evaluation.origin;
import auto_js;
import std;
import v8;

namespace js::iv8 {

export class script : public v8::UnboundScript {
	public:
		// nb: It is undocumented (and even mentions "context independent"), but the script compiler
		// actually needs a context because it can throw an error and *that* would need a context.
		static auto compile(context_lock_witness lock, auto code_string, source_origin source_origin) -> v8::MaybeLocal<script>;
		auto run(context_lock_witness lock) -> v8::Local<v8::Value>;

	private:
		static auto compile(context_lock_witness lock, v8::Local<v8::String> code_string, source_origin source_origin) -> v8::MaybeLocal<script>;
};

// ---

auto script::compile(context_lock_witness lock, auto code_string, source_origin source_origin) -> v8::MaybeLocal<script> {
	auto local_code_string = js::transfer_in_strict<v8::Local<v8::String>>(std::move(code_string), lock);
	return compile(lock, local_code_string, std::move(source_origin));
}

} // namespace js::iv8

module isolated_vm:support.callback_info;
import :support.callback_info_fwd;
import :support.cast;
import v8;

namespace isolated_vm {

auto callback_info::size() const -> int {
	const auto& info = *std::bit_cast<const v8::FunctionCallbackInfo<v8::Value>*>(info_);
	return info.Length();
}

auto callback_info::iterator::operator*() const -> value_type {
	const auto& info = *std::bit_cast<const v8::FunctionCallbackInfo<v8::Value>*>(info_);
	return cast_out(info[ index_ ]);
}

class callback_info_implementation : public callback_info {
	public:
		explicit callback_info_implementation(const v8::FunctionCallbackInfo<v8::Value>& info) :
				callback_info{&info} {};
};

} // namespace isolated_vm

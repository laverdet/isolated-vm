module ivm.napi;
import :handle;
import :utility;

namespace ivm::js::napi {

handle::handle(napi_env env, napi_value value) :
		env_{env},
		value_{value} {};

handle::operator napi_value() const {
	return **this;
}

auto handle::operator*() const -> napi_value {
	return value_;
}

auto handle::operator==(const handle& right) const -> bool {
	return value_ == right.value_;
}

auto handle::env() const -> napi_env {
	return env_;
}

} // namespace ivm::js::napi

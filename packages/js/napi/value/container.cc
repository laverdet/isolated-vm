module ivm.napi;
import :container;
import :utility;

namespace ivm::js::napi {

container::container(napi_env env, napi_value value) :
		env_{env},
		value_{value} {};

container::operator napi_value() const {
	return **this;
}

auto container::operator*() const -> napi_value {
	return value_;
}

auto container::operator==(const container& right) const -> bool {
	return value_ == right.value_;
}

auto container::env() const -> napi_env {
	return env_;
}

} // namespace ivm::js::napi

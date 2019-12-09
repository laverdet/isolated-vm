#pragma once
#include <v8.h>
#include <memory>

namespace ivm {
namespace detail {

class TransferableOptions {
	public:
		enum class Type { None, Copy, ExternalCopy, Reference };

		TransferableOptions() = default;
		explicit TransferableOptions(v8::Local<v8::Object> options, Type fallback = Type::None);
		explicit TransferableOptions(v8::MaybeLocal<v8::Object> maybe_options, Type fallback = Type::None);

		bool operator==(const TransferableOptions& that) const {
			return type == that.type && fallback == that.fallback;
		}

		Type type = Type::None;
		Type fallback = Type::None;

	private:
		void ParseOptions(v8::Local<v8::Object> options);
};

} // namespace detail

class Transferable {
	public:
		using Options = detail::TransferableOptions;
		Transferable() = default;
		Transferable(const Transferable&) = delete;
		auto operator= (const Transferable&) = delete;
		virtual ~Transferable() = default;
		virtual auto TransferIn() -> v8::Local<v8::Value> = 0;
		static auto OptionalTransferOut(v8::Local<v8::Value> value, Options options = {}) -> std::unique_ptr<Transferable>;
		static auto TransferOut(v8::Local<v8::Value> value, Options options = {}) -> std::unique_ptr<Transferable>;
};

} // namespace ivm

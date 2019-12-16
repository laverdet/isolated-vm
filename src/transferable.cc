#include "isolate/class_handle.h"
#include "isolate/util.h"
#include "reference_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "external_copy.h"
#include "external_copy_handle.h"

using namespace v8;

namespace ivm {
namespace detail {

TransferableOptions::TransferableOptions(Local<Object> options, Type fallback) : fallback{fallback} {
	ParseOptions(options);
}

TransferableOptions::TransferableOptions(MaybeLocal<Object> maybe_options, Type fallback) : fallback{fallback} {
	Local<Object> options;
	if (maybe_options.ToLocal(&options)) {
		ParseOptions(options);
	}
}

void TransferableOptions::TransferableOptions::ParseOptions(Local<Object> options) {
	bool copy = ReadOption<bool>(options, "copy", false);
	bool externalCopy = ReadOption<bool>(options, "externalCopy", false);
	bool reference = ReadOption<bool>(options, "reference", false);
	if ((copy && externalCopy) || (copy && reference) || (externalCopy && reference)) {
		throw RuntimeTypeError("Only one of `copy`, `externalCopy`, or `reference` may be set");
	}
	if (copy) {
		type = Type::Copy;
	} else if (externalCopy) {
		type = Type::ExternalCopy;
	} else if (reference) {
		type = Type::Reference;
	}
}

} // namespace detail

auto Transferable::OptionalTransferOut(Local<Value> value, Options options) -> std::unique_ptr<Transferable> {
	auto TransferWithType = [&](Options::Type type) -> std::unique_ptr<Transferable> {
		switch (type) {
			default:
				return nullptr;

			case Options::Type::Copy:
				return ExternalCopy::Copy(value);

			case Options::Type::ExternalCopy:
				return std::make_unique<ExternalCopyHandle::ExternalCopyTransferable>(ExternalCopy::Copy(value));

			case Options::Type::Reference:
				return std::make_unique<ReferenceHandleTransferable>(value);
		}
	};

	switch (options.type) {
		case Options::Type::None: {
			if (value->IsObject()) {
				auto ptr = ClassHandle::Unwrap<TransferableHandle>(value.As<Object>());
				if (ptr != nullptr) {
					return ptr->TransferOut();
				}
			}
			auto result = ExternalCopy::CopyIfPrimitive(value);
			if (result) {
				return result;
			}
			return TransferWithType(options.fallback);
		}

		default:
			return TransferWithType(options.type);
	}
}

auto Transferable::TransferOut(Local<Value> value, Options options) -> std::unique_ptr<Transferable> {
	auto copy = OptionalTransferOut(value, options);
	if (!copy) {
		throw RuntimeTypeError("A non-transferable value was passed");
	}
	return copy;
}

} // namespace ivm

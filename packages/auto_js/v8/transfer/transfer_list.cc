export module v8_js:transfer_list;
import :array;
import :handle.value;
import :object;
import :lock;
import :unmaybe;
import :value.tag;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

// Transfer delegate for `ArrayBuffer`
export class array_buffer_transfer {
	public:
		array_buffer_transfer(context_lock_witness /*lock*/, std::vector<v8::Local<v8::Value>>& entries) :
				buffers_{[ & ] -> auto {
					constexpr auto maybe_claim_in = [](v8::Local<v8::Value> value) -> std::optional<v8::Local<v8::ArrayBuffer>> {
						if (value->IsArrayBuffer()) {
							auto buffer = value.As<v8::ArrayBuffer>();
							if (!buffer->IsDetachable()) {
								throw js::type_error{u"ArrayBuffer is not detachable"};
							}
							return buffer;
						} else {
							return std::nullopt;
						}
					};
					return js::extract_transferees(util::cw<u"ArrayBuffer">, maybe_claim_in, std::equal_to{}, entries);
				}()} {}

		template <class Visit, class Accept>
		auto operator()(v8::Local<v8::ArrayBuffer> subject, Visit& visit, const Accept& accept) {
			static_assert(js::accept_allows_throw<Accept>);
			auto claim_out = [ & ](v8::Local<v8::ArrayBuffer> handle) -> accept_target_t<Accept> {
				auto buffer = [ & ] -> js::array_buffer {
					if (handle->ByteLength() == 0) {
						return js::array_buffer{std::span<std::byte>{}};
					} else {
						// Steal the backing store, avoiding a copy of the contents
						auto backing_store = std::make_unique<std::shared_ptr<v8::BackingStore>>(handle->GetBackingStore());
						auto data = std::span{static_cast<std::byte*>((*backing_store)->Data()), (*backing_store)->ByteLength()};
						return js::array_buffer{
							data,
							js::array_buffer::deleter{
								[](std::shared_ptr<v8::BackingStore>* backing_store, std::byte* /*data*/) -> void {
									delete backing_store;
								},
								backing_store.release()
							}
						};
					}
				}();
				unmaybe(handle->Detach(v8::Local<v8::Value>{}));
				return accept(array_buffer_tag{}, visit, js::transferred_value{v8::Local<v8::Data>{handle}, buffer});
			};
			return js::claim_transferee(buffers_, subject, std::equal_to{}, claim_out);
		}

		auto finalize() -> void {
			for (auto entry : std::exchange(buffers_, {})) {
				unmaybe(entry->Detach(v8::Local<v8::Value>{}));
			}
		}

	private:
		std::vector<v8::Local<v8::ArrayBuffer>> buffers_;
};

// v8 'transferList' delegate
export template <class... Delegates>
auto transfer_list(context_lock_witness lock, std::optional<value_of<list_tag>> list) {
	auto entries = list ? std::vector{std::from_range, value_for_array{lock, list->template As<v8::Array>()}} : std::vector<v8::Local<v8::Value>>{};
	auto delegate = js::visitor_delegate_of{Delegates{lock, entries}...};
	if (!entries.empty()) {
		throw js::type_error{u"Transfer list contains unknown value"};
	}
	return delegate;
}

} // namespace js::iv8

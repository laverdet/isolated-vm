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
			return maybe_claim_out(subject, visit, accept);
		}

		template <class Visit, class Accept>
		auto operator()(v8::Local<iv8::DataBlock> subject, Visit& visit, const Accept& accept) {
			return maybe_claim_out(subject, visit, accept);
		}

		auto finalize() -> void {
			for (auto entry : std::exchange(buffers_, {})) {
				unmaybe(entry->Detach(v8::Local<v8::Value>{}));
			}
		}

	private:
		template <class Visit, class Accept>
		auto maybe_claim_out(auto subject, Visit& visit, const Accept& accept) {
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

		std::vector<v8::Local<v8::ArrayBuffer>> buffers_;
};

// v8 'transferList' delegate
export template <class... Delegates>
class transfer_list {
	public:
		template <class Type>
		transfer_list(context_lock_witness lock, std::optional<Type> list) :
				transfer_list{lock, list ? entries_of(lock, list->template As<v8::Array>()) : std::vector<v8::Local<v8::Value>>{}} {}

		transfer_list(context_lock_witness lock, value_of<list_tag> list) :
				transfer_list{lock, entries_of(lock, list.As<v8::Array>())} {}

		// Constructs a list in place and invokes the given operation with it, finalizing afterwards
		static auto with(context_lock_witness lock, auto list, auto operation) {
			auto self = transfer_list{lock, std::move(list)};
			auto result = operation(self);
			self.finalize();
			return result;
		}

		// Offers the subject to each delegate in order, claiming with the first which engages
		template <class Visit, class Accept>
		auto operator()(auto subject, Visit& visit, const Accept& accept) -> std::optional<accept_target_t<Accept>> {
			return util::template_traverse(
				util::sequence_cw<sizeof...(Delegates)>,
				util::overloaded{
					[ & ](auto ii, auto next) -> std::optional<accept_target_t<Accept>> {
						auto& delegate = std::get<ii>(delegates_);
						if constexpr (std::invocable<decltype(delegate), decltype(subject), Visit&, const Accept&>) {
							if (auto claim = delegate(subject, visit, accept)) {
								return std::optional{(*std::move(claim))()};
							}
						}
						return next();
					},
					[] -> std::optional<accept_target_t<Accept>> { return std::nullopt; },
				}
			);
		}

		auto finalize() -> void {
			constexpr auto [... ii ] = util::sequence<sizeof...(Delegates)>;
			(..., std::get<ii>(delegates_).finalize());
		}

	private:
		// Each delegate claims its entries from the vector. Anything left over is unknown.
		transfer_list(context_lock_witness lock, std::vector<v8::Local<v8::Value>> entries) :
				delegates_{Delegates{lock, entries}...} {
			if (!entries.empty()) {
				throw js::type_error{u"Transfer list contains unknown value"};
			}
		}

		static auto entries_of(context_lock_witness lock, v8::Local<v8::Array> list) -> std::vector<v8::Local<v8::Value>> {
			return std::vector{std::from_range, value_for_array{lock, list}};
		}

		std::tuple<Delegates...> delegates_;
};

} // namespace js::iv8

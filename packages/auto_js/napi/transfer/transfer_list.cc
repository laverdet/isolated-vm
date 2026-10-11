export module napi_js:transfer_list;
import :lock;
import :support.host;
import :utility;
import :value;
import std;
import util;

namespace js::napi {

// Transfer delegate for `ArrayBuffer`
export class array_buffer_transfer {
	public:
		array_buffer_transfer(environment_lock_witness lock, std::vector<local_of<>>& entries) :
				lock_{lock},
				buffers_{[ & ] -> auto {
					auto maybe_claim_in = [ & ](napi_value value) -> std::optional<local_of<array_buffer_tag>> {
						if (is_object_array_buffer(napi_env{lock}, local_of<object_tag>::from(value))) {
							return local_of<array_buffer_tag>::from(value);
						} else {
							return std::nullopt;
						}
					};
					return js::extract_transferees(util::cw<u"ArrayBuffer">, maybe_claim_in, addressof_equal{}, entries);
				}()} {}

		template <class Visit, class Accept>
		auto operator()(local_of<array_buffer_tag> subject, Visit& visit, const Accept& accept) {
			static_assert(js::accept_allows_throw<Accept>);
			auto claim_out = [ & ](local_of<array_buffer_tag> handle) -> accept_target_t<Accept> {
				auto value = value_of{lock_, handle};
				auto buffer = [ & ] -> js::array_buffer {
					if (array_buffer_get_backing_store == nullptr || value.byte_length() == 0) {
						return js::array_buffer{std::span<std::byte>{value}};
					} else {
						// Steal the backing store, avoiding a copy of the contents
						auto backing_store = std::make_unique<std::shared_ptr<data_block::array_type>>(array_buffer_get_backing_store(local_of{value}));
						return js::array_buffer{
							std::span<std::byte>{value},
							js::array_buffer::deleter{
								[](std::shared_ptr<data_block::array_type>* backing_store, std::byte* /*data*/) -> void {
									delete backing_store;
								},
								backing_store.release()
							}
						};
					}
				}();
				value.detach();
				return accept(array_buffer_tag{}, visit, js::transferred_value{napi_value{handle}, buffer});
			};
			return js::claim_transferee(buffers_, subject, addressof_equal{}, claim_out);
		}

		// Detaches listed buffers which were never claimed by the visitor
		auto finalize() -> void {
			for (auto entry : std::exchange(buffers_, {})) {
				value_of{lock_, entry}.detach();
			}
		}

	private:
		environment_lock_witness lock_;
		std::vector<local_of<array_buffer_tag>> buffers_;
};

// Napi 'transferList' delegate
export template <class... Delegates>
auto transfer_list(const auto& lock, std::optional<value_of<list_tag>> list) {
	auto entries = list ? std::vector{std::from_range, list->values()} : std::vector<local_of<>>{};
	auto delegate = js::visitor_delegate_of{Delegates{lock, entries}...};
	if (!entries.empty()) {
		throw js::type_error{u"Transfer list contains unknown value"};
	}
	return delegate;
}

} // namespace js::napi

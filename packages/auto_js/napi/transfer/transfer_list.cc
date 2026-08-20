export module napi_js:transfer_list;
import :support.host;
import :utility;
import :value;
import std;
import util;

namespace js::napi {

// Transfer delegate for `ArrayBuffer`
export class array_buffer_transfer {
	public:
		array_buffer_transfer() = default;

		array_buffer_transfer(auto_environment auto& env, std::vector<napi_value>& entries) :
				env_{napi_env{env}},
				buffers_{js::extract_transferees(
					util::cw<u"ArrayBuffer">,
					[ env = env_ ](napi_value value) -> std::optional<local_of<array_buffer_tag>> {
						if (is_object_array_buffer(env, local_of<object_tag>::from(value))) {
							return local_of<array_buffer_tag>::from(value);
						} else {
							return std::nullopt;
						}
					},
					addressof_equal{},
					entries
				)} {}

		template <class Visit, class Accept>
		auto operator()(local_of<object_tag> subject, Visit& visit, const Accept& accept) {
			return maybe_claim(subject, visit, accept);
		}

		template <class Visit, class Accept>
		auto operator()(local_of<data_block_tag> subject, Visit& visit, const Accept& accept) {
			return maybe_claim(subject, visit, accept);
		}

		// Detaches listed buffers which were never claimed by the visitor
		auto finalize() -> void {
			for (auto entry : std::exchange(buffers_, {})) {
				value_of{env_, entry}.detach();
			}
		}

	private:
		template <class Visit, class Accept>
		auto maybe_claim(auto subject, Visit& visit, const Accept& accept) {
			auto it = std::ranges::find(buffers_, handle_addressof(napi_value{subject}), handle_addressof);
			return it == buffers_.end()
				? std::nullopt
				: std::optional{[ this, it, &visit, &accept ] -> accept_target_t<Accept> {
						auto handle = std::exchange(*it, buffers_.back());
						buffers_.pop_back();
						auto value = value_of{env_, handle};
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
					}};
		}

		napi_env env_{};
		std::vector<local_of<array_buffer_tag>> buffers_;
};

// Napi 'transferList' delegate
export template <class... Delegates>
class transfer_list {
	public:
		transfer_list() = default;

		template <class Type>
		transfer_list(auto_environment auto& env, std::optional<Type> list) :
				transfer_list{list ? transfer_list{env, *std::move(list)} : transfer_list{}} {}

		template <class Type>
		transfer_list(auto_environment auto& env, js::forward<Type> list) :
				transfer_list{env, *list} {}

		transfer_list(auto_environment auto& env, local_of<list_tag> list) :
				delegates_{[ & ] {
					auto entries = value_of{napi_env{env}, local_of<vector_tag>::from(list)} | std::ranges::to<std::vector<napi_value>>();
					auto delegates = std::tuple{Delegates{env, entries}...};
					if (!entries.empty()) {
						throw js::type_error{u"Transfer list contains unknown value"};
					}
					return delegates;
				}()} {}

		// Constructs a list in place and invokes the given operation with it, finalizing afterwards
		static auto with(auto_environment auto& env, auto list, auto operation) {
			auto self = transfer_list{env, std::move(list)};
			auto result = operation(self);
			self.finalize();
			return result;
		}

		// Offers the subject to each delegate in order, claiming with the first which engages
		template <class Visit, class Accept>
		auto operator()(auto subject, Visit& visit, const Accept& accept) -> std::optional<accept_target_t<Accept>> {
			return util::template_traverse(
				util::sequence<sizeof...(Delegates)>,
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
		std::tuple<Delegates...> delegates_;
};

} // namespace js::napi

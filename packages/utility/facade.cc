export module ivm.utility:facade;

export template <class Type, class difference_type_, class wide_size_type = difference_type_>
class arithmetic_facade {
	public:
		using difference_type = difference_type_;

		// Type -= difference_type
		auto operator-=(this auto& self, difference_type offset) -> decltype(auto) { return self += -offset; }

		// ++Type / --Type
		auto operator++(this auto& self) -> decltype(auto) { return self += 1; }
		auto operator--(this auto& self) -> decltype(auto) { return self -= 1; }

		// Type++ / Type--
		auto operator++(this auto& self, int) {
			auto result = self;
			++self;
			return result;
		}
		auto operator--(this auto& self, int) {
			auto result = self;
			--self;
			return result;
		}

		// Type +/- difference_type
		auto operator+(this const auto& self, difference_type offset) {
			auto result = self;
			return result += offset;
		}
		auto operator-(this const auto& self, difference_type offset) {
			auto result = self;
			return result -= offset;
		}

		// difference_type + Type
		friend auto operator+(difference_type left, const Type& right) {
			return right + left;
		}

		// Type - Type
		auto operator-(this auto&& self, decltype(self) right) {
			return static_cast<difference_type>(wide_size_type{+self} - wide_size_type{+right});
		}
};

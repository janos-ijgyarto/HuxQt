#pragma once
#include <type_traits>

namespace HuxApp
{
	namespace Utils
	{
		template<typename ENUM>
		constexpr auto to_integral(ENUM e) -> typename std::underlying_type<ENUM>::type
		{
			return static_cast<typename std::underlying_type<ENUM>::type>(e);
		}

		template<typename ENUM, typename VALUE>
		constexpr ENUM to_enum(VALUE value)
		{
            static_assert(std::is_enum_v<ENUM>);
			return static_cast<ENUM>(value);
		}
	}
}

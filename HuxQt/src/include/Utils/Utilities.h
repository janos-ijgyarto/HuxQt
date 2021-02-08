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
	}
}
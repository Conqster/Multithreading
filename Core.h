#pragma once
#include <iostream>


#define LOG_MSG(x) std::cout<<x
#define ASSERT _ASSERT


#define PROFILE_SCOPE ZoneScoped


class NonCopyable
{
public: 
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	void  operator=(const NonCopyable&) = delete;
};


namespace Utils{
	template<typename T>
	constexpr T WrapPowerof2(const T& value, const T& end)
	{
		return value & end;
	}
}

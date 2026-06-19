#pragma once
#include <iostream>


#define LOG_MSG(x) std::cout<<x
#define ASSERT _ASSERT

class NonCopyable
{
public: 
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	void  operator=(const NonCopyable&) = delete;
};
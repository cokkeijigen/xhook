#include <iostream>
#include <print>
#include <windows.h>
#include <xhook.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#define __noinline __declspec(noinline)
#else
#define __noinline __attribute__((noinline))
#endif

namespace example09
{
	class my_class 
	{
	public:

		int value;

		my_class(int _value) noexcept : value{ _value }
		{
		}

		__noinline auto method1(int a, int b) noexcept -> int
		{
			this->value += a + b;
			std::println("[my_class::method1] value: {} a: {:02} b: {:02}", this->value, a, b);
			return this->value;
		}
	};
	
	extern "C" auto main(int, char**) -> int 
	{
		std::println("Test example09");

		auto result = xhook::ptr{ &my_class::method1 } ->* xhook::thiscall
		{
			[]<xhook::backup auto backup>(my_class* m_this, int a, int b) -> int
			{
				m_this->value = a * b;
				std::println("[_my_hook::method1] value: {} a: {:02} b: {:02}", m_this->value, a, b);
				return backup(m_this, 1, 2);
			}
		};
		
		my_class my_clazz{ 10 };
		my_clazz.method1(20, 30);

		(*result).unhook();
		my_clazz.method1(40, 50);

		return {};
	}
}
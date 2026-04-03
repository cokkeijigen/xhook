#include <iostream>
#include <print>
#include <windows.h>
#include <xhook.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#define noinline __declspec(noinline)
#else
#define noinline __attribute__((noinline))
#endif

namespace example08
{
	class my_class 
	{
		int value;

	public:

		inline my_class(int _value) noexcept : value{ _value }
		{
		}

		noinline auto method1(int a, int b) noexcept -> int
		{
			this->value += a + b;
			std::println("[my_class::method1] value: {} a: {:02} b: {:02}", this->value, a, b);
			return this->value;
		}
	};

	class my_hook 
	{
		int value;

	public:

		noinline auto method1(int a, int b) noexcept -> int
		{
			this->value = a * b;
			std::println("[_my_hook::method1] value: {} a: {:02} b: {:02}", this->value, a, b);
			return (this->*xhook::hooker::call<&my_hook::method1>)(1, 2);
		}
	};
	
	extern "C" auto main(int, char**) -> int 
	{
		xhook::hooker::add<&my_hook::method1>(&my_class::method1);

		std::println("Test example08");
		my_class my_clazz{ 10 };
		my_clazz.method1(20, 30);

		xhook::hooker::del<&my_hook::method1>();
		
		my_clazz.method1(40, 50);

		return {};
	}
}
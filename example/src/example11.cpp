#include <iostream>
#include <print>
#include <windows.h>
#include <xhook.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#define __noinline __declspec(noinline)
#else
#define __noinline __attribute__((noinline))
#endif

namespace example11
{

	struct my_hooks
	{
		// Can be replaced with `using namespace xhook::call;`, but this usage is not allowed within a class scope.
		template<xhook::is_function_pointer auto func>
		using call = xhook::invoke<func>;

		static int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
		{
			auto msg{ std::string{ "[Hooked] " }.append(lpText)    };
			auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
			return hooks->*call<my_hooks::MessageBoxA>{ hWnd, msg.data(), cap.data(), uType };
		}

		static int WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) noexcept
		{
			auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
			auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
			return hooks->*call<my_hooks::MessageBoxW>{ hWnd, msg.data(), cap.data(), uType };
		}

		__noinline auto my_method_hook(int a, int b) noexcept -> int
		{
			this->value = a * b;
			std::println("[my_hooks::my_method_hook] value: {} a: {:02} b: {:02}", this->value, a, b);

			return hooks->*call<&my_hooks::my_method_hook>(this, 11, 45);
		}
		
		static inline xhook::hooks hooks
		{
			xhook::ptr{ ::MessageBoxA } && xhook::target<my_hooks::MessageBoxA>{},
			xhook::ptr{ ::MessageBoxW } && xhook::target<my_hooks::MessageBoxW>{},
			xhook::target<&my_hooks::my_method_hook>{}
		};

		int value;
	};

	class my_class 
	{
	public:

		int value;
		
		__noinline auto my_method(int a, int b) noexcept -> int 
		{
			this->value += a + b;
			std::println("[my_class::my_method] value: {} a: {:02} b: {:02}", this->value, a, b);
			return this->value;
		}

	};

	extern "C" auto main(int, char**) -> int 
	{
		my_hooks::hooks.set<&my_hooks::my_method_hook>(&my_class::my_method);
		my_hooks::hooks.commits();

		my_class obj{ 114 };
		obj.my_method(11, 22);
		
		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example11", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example11", MB_YESNO | MB_ICONQUESTION);
		
		my_hooks::hooks.unhooks();

		obj.my_method(33, 44);
		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example11", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example11", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
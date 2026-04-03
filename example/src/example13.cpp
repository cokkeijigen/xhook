#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example13
{
	struct my_hooks
	{
		template<auto func>
		requires xhook::is_function_pointer<decltype(func)>
		// Can be replaced with `using namespace xhook::call;`, but this usage is not allowed within a class scope.
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

		// win10 22h2 user32.dll
		static inline xhook::rva_hooks hooks
		{
			#ifdef _M_IX86
			xhook::rva{ 0x806A0 } && xhook::target<my_hooks::MessageBoxA>{},
			xhook::rva{ 0x80BC0 } && xhook::target<my_hooks::MessageBoxW>{},
			#else	
			xhook::rva{ 0x78B70 } && xhook::target<my_hooks::MessageBoxA>{},
			xhook::rva{ 0x791F0 } && xhook::target<my_hooks::MessageBoxW>{},
			#endif
		};

	};

	extern "C" auto main(int, char**) -> int
	{
		// or my_hooks::hooks.commit(xhook::get_base(L"User32.dll"));
		if (my_hooks::hooks.commits(L"User32.dll")) 
		{
			// success...
		}

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example13", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example13", MB_YESNO | MB_ICONQUESTION);

		my_hooks::hooks.unhooks();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example13", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example13", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
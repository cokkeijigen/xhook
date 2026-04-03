#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example06
{
	using namespace xhook::call;
	using namespace xhook::sym;

	struct my_hooks
	{
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

		static inline xhook::symbol_hooks hooks
		{
			"MessageBoxA"_sym && xhook::target<my_hooks::MessageBoxA>{},
			"MessageBoxW"_sym && xhook::target<my_hooks::MessageBoxW>{},
		};

	};

	extern "C" auto main(int, char**) -> int
	{
		my_hooks::hooks.commits(L"User32.dll");
		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example06", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example06", MB_YESNO | MB_ICONQUESTION);

		my_hooks::hooks.unhooks();
		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example06", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example06", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
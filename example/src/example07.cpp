#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example07
{

	extern "C" auto main(int, char**) -> int
	{
		using namespace xhook::sym;
		using namespace xhook::dll;

		xhook::xmodule user32_dll = L"User32"_dll;

		auto msgboxa = user32_dll & "MessageBoxA"_sym ->* xhook::winapi  // or xhook::stdcall
		{
			[]<xhook::backup auto backup>
			(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
			{
				auto msg{ std::string{ "[Hooked] " }.append(lpText)    };
				auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
				return backup(hWnd, msg.data(), cap.data(), uType);
			}
		};
		
		auto msgboxw = user32_dll & "MessageBoxW"_sym ->* xhook::winapi // or xhook::stdcall
		{
			[]<xhook::backup auto backup>
			(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
			{
				auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
				auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
				return backup(hWnd, msg.data(), cap.data(), uType);
			}
		};

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example07", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example07", MB_YESNO | MB_ICONQUESTION);
		
		(*msgboxw).unhook();
		(*msgboxa).unhook();
		
		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example07", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example07", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
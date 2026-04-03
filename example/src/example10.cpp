#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example10
{

	extern "C" auto main(int, char**) -> int
	{
		auto msgboxw = xhook::ptr{ ::MessageBoxW } ->* xhook::winapi
		{
			[] <xhook::backup auto backup>
			(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
			{
				auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
				auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
				return backup(hWnd, msg.data(), cap.data(), uType);
			}
		};

		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example10", MB_YESNO | MB_ICONQUESTION);

		(*msgboxw).unhook();

		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example10", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example03
{
	static auto WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::string{ "[Hooked] " }.append(lpText)    };
		auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
		return xhook::target<example13::MessageBoxA>::call(hWnd, msg.data(), cap.data(), uType);
	}

	static auto WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
		auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
		return xhook::target<example13::MessageBoxW>::call(hWnd, msg.data(), cap.data(), uType);
	}

	extern "C" auto main(int, char**) -> int 
	{
		// win10 22h2 user32.dll
		xhook::xmodule user32_dll = "User32.dll";

		#ifdef _M_IX86
		user32_dll += xhook::rva{ 0x806A0 } && xhook::target<example13::MessageBoxA>{};
		user32_dll += xhook::rva{ 0x80BC0 } && xhook::target<example13::MessageBoxW>{};
		#else	
		user32_dll += xhook::rva{ 0x78B70 } && xhook::target<example13::MessageBoxA>{};
		user32_dll += xhook::rva{ 0x791F0 } && xhook::target<example13::MessageBoxW>{};
		#endif

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example03", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example03", MB_YESNO | MB_ICONQUESTION);

		xhook::target<example13::MessageBoxA>::unhook();
		xhook::target<example13::MessageBoxW>::unhook();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example03", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example03", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
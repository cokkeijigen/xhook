#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example04
{

	static auto WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::string{ "[Hooked] " }.append(lpText) };
		auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
		return xhook::target<example04::MessageBoxA>::call(hWnd, msg.data(), cap.data(), uType);
	}

	static auto WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::wstring{ L"[Hooked] " }.append(lpText) };
		auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
		return xhook::target<example04::MessageBoxW>::call(hWnd, msg.data(), cap.data(), uType);
	}

	extern "C" auto main(int, char**) -> int
	{
		using namespace xhook::sym;
		using namespace xhook::dll;

		xhook::xmodule user32_dll = L"User32"_dll;
		user32_dll += "MessageBoxA"_sym ->* xhook::target<example04::MessageBoxA>{};
		user32_dll += "MessageBoxW"_sym ->* xhook::target<example04::MessageBoxW>{};

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example04", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example04", MB_YESNO | MB_ICONQUESTION);

		xhook::target<example04::MessageBoxA>::unhook();
		xhook::target<example04::MessageBoxW>::unhook();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example04", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example04", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
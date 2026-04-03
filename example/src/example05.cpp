#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example05
{

	static auto WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::string{ "[Hooked] " }.append(lpText) };
		auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
		return xhook::target<example05::MessageBoxA>::call(hWnd, msg.data(), cap.data(), uType);
	}

	static auto WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::wstring{ L"[Hooked] " }.append(lpText) };
		auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
		return xhook::target<example05::MessageBoxW>::call(hWnd, msg.data(), cap.data(), uType);
	}

	extern "C" auto main(int, char**) -> int
	{
		using namespace xhook::sym;
		using namespace xhook::dll;

		const auto user32_dll = L"User32"_dll;
		user32_dll.add_hook("MessageBoxA"_sym->*xhook::target<example05::MessageBoxA>{});

		std::string msgw{ "MessageBoxW" };
		user32_dll.add_hook(xhook::symstr{ msgw }->*xhook::target<example05::MessageBoxW>{});

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example05", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example05", MB_YESNO | MB_ICONQUESTION);

		xhook::target<example05::MessageBoxA>::unhook();
		xhook::target<example05::MessageBoxW>::unhook();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example05", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example05", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
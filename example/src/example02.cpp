#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example02
{

	static auto WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::string{ "[Hooked] " }.append(lpText)    };
		auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
		return xhook::hooker::call<example02::MessageBoxA>(hWnd, msg.data(), cap.data(), uType);
	}

	static auto WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
		auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
		return xhook::hooker::call<example02::MessageBoxW>(hWnd, msg.data(), cap.data(), uType);
	}

	extern "C" auto main(int, char**) -> int
	{
		xhook::hooker::add<example02::MessageBoxA>(::MessageBoxA, false);
		xhook::hooker::add<example02::MessageBoxW>(::MessageBoxW, false);
		xhook::hooker::commit();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example02", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example02", MB_YESNO | MB_ICONQUESTION);
		
		xhook::hooker::del<example02::MessageBoxA>(false);
		xhook::hooker::del<example02::MessageBoxW>(false);
		xhook::hooker::commit();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example02", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example02", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
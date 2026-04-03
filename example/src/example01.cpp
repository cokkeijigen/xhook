#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace example01 
{

	static auto WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::string{ "[Hooked] " }.append(lpText)    };
		auto cap{ std::string{ "[Hooked] " }.append(lpCaption) };
		return xhook::hooker::call<example01::MessageBoxA>(hWnd, msg.data(), cap.data(), uType);
	}

	static auto WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
	{
		auto msg{ std::wstring{ L"[Hooked] " }.append(lpText)    };
		auto cap{ std::wstring{ L"[Hooked] " }.append(lpCaption) };
		return xhook::hooker::call<example01::MessageBoxW>(hWnd, msg.data(), cap.data(), uType);
	}

	extern "C" auto main(int, char**) -> int 
	{
		xhook::hooker::add<example01::MessageBoxA>(::MessageBoxA);
		xhook::hooker::add<example01::MessageBoxW>(::MessageBoxW);

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example01", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example01", MB_YESNO | MB_ICONQUESTION);

		xhook::hooker::del<example01::MessageBoxA>();
		xhook::hooker::del<example01::MessageBoxW>();

		::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example01", MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example01", MB_YESNO | MB_ICONQUESTION);

		return {};
	}
}
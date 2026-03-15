#include <iostream>
#include <windows.h>
#include <xhook.hpp>

namespace xhook_test 
{

	static __declspec(noinline) auto __fastcall hello(const char* str1, const char* str2) -> int
	{
		std::cout << str1 << str2 << std::endl;
		return 0x114514;
	}

	extern "C" auto main(int, char**) -> int
	{

		std::string str0 = "书山有路搞为径，学海无涯基佬作舟！";
		auto ptr1 = xhook::add_hook
		(
			reinterpret_cast<void*>(hello), 
			xhook::fastcall
			{
				[&str0]<xhook::backup auto backup>(const char* str1, const char* str2) -> int
				{
					std::cout << "[hooked] " << str1 << std::endl;
					std::cout << "[hooked] " << str2 << std::endl;
					std::cout << "[hooked] capture: " << std::hex << str0 << std::endl;
					return backup(str1, str2);
				}
			}
		);

		std::cout << "ptr1 -> 0x" << std::hex << ptr1 << std::endl;

		int result1 = hello("总之打点字上去", "喵喵喵");
		std::cout << "result1 -> 0x" << std::hex << result1 << "\n\n";

		using namespace xhook::sym;
		using namespace xhook::dll;

		//xhook::xmodule user32_dll = L"User32"_dll;
		const auto user32_dll = L"User32"_dll;

		user32_dll += "MessageBoxExW"_sym ->* xhook::winapi
		{
			[]<xhook::backup auto backup>
			(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId) static -> int
			{
				std::wstring msg = L"[hooked] ";
				return backup(hWnd, msg.append(lpText).data(), lpCaption, uType, wLanguageId);
			}
		};

		auto orgMessageBoxW = user32_dll.add_hook
		(
			"MessageBoxW"_sym ->* xhook::winapi
			{
				[]<xhook::backup auto backup>
				(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) static -> int
				{
					std::wstring msg = L"[hooked] ";
					return backup(hWnd, msg.append(lpText).data(), lpCaption, uType);
				}
			}
		);

		auto orgMessageBoxA = user32_dll & "MessageBoxA"_sym ->* xhook::winapi
		{
			[]<xhook::backup auto backup>
			(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) static -> int
			{
				std::string msg = "[hooked] ";
				return backup(hWnd, msg.append(lpText).data(), lpCaption, uType);
			}
		};

		::MessageBoxA  (NULL,  "总之打点字上去",  "喵喵喵",  MB_YESNO | MB_ICONQUESTION);
		::MessageBoxW  (NULL, L"你好世界喵呜喵", L"喵呜喵",  MB_YESNO | MB_ICONQUESTION);
		::MessageBoxExW(NULL, L"Hello World", L"Nya~~~~", MB_YESNO | MB_ICONQUESTION, 
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
		);

		return {};
	}
}
# XHook — A ~~useless toy~~ utility library built with Modern C++

<div align="center"> 
	<a href="https://github.com/cokkeijigen/xhook/blob/master/README.md">简体中文</a> | 
	<a href="https://github.com/cokkeijigen/xhook/blob/master/README_EN.md">English</a> <br>
	Just a little something I wrote for fun, <del>anyway, no one will use the garbage I write (just kidding)</del><br>
</div>

![](https://github.com/cokkeijigen/xhook/blob/master/image.jpg?raw=true)
**Warning: This project is in an early development stage. Subsequent updates may introduce destructive changes. It is not advised for use in any formal or production projects.**

# How to use (requires cmake and vcpkg)

Add detours to the dependencies in your project's vcpkg.json

```json
{
  "name": "your-project-name",
  "version": "1.0.0",
  "dependencies": [
    "detours",
	...
  ],
   ...
  }
}
```

Add to `CMakeLists.txt`

```cmake
set(CMAKE_CXX_STANDARD 23) #cpp23 recommended

include(FetchContent)
FetchContent_Declare(
    xhook
    GIT_REPOSITORY https://github.com/cokkeijigen/xhook.git
    GIT_TAG master
)
FetchContent_MakeAvailable(xhook)

...

target_link_libraries(YOUR_PROJECT_NAME
	xhook::xhook
	...
)
```

Or use `git clone -b master https://github.com/cokkeijigen/xhook.git` to clone into your project folder, then `add_subdirectory(xhook)` also works.

## `xhook::ptr ->* xhook::target` and `xhook::target::call`

```cpp
// Add hook
xhook::ptr{ /* target function address */ } ->* xhook::target</* replacement function as template parameter */>{};

// Call original function
xhook::target</* replacement function as template parameter */>::call(.....);

// Remove hook
xhook::target</* replacement function as template parameter */>::unhook();
```

```cpp
#include <iostream>
#include <xhook.hpp>

static __declspec(noinline) auto __fastcall test(int v1, int v2, int v3) -> int
{
	int result = v1 + v2 + v3;
	std::cout << "test: " << result << std::endl;
	return result;
}

static __declspec(noinline) auto __fastcall test_hook(int v1, int v2, int v3) -> int
{
	std::cout << "call test_hook" << std::endl;
	return xhook::target<test_hook>::call(v1, v2, v3); // Call original function
}

auto main(int, char**) -> int
{
    // Add hook
	xhook::ptr{ test } ->* xhook::target<test_hook>{};

	// Test
	test(11, 45, 14);

    // Remove hook
    xhook::target<test_hook>::unhook();
	return {};
}
```

## `xhook::hooker::add_hook` and `xhook::hooker::call`

```cpp
// Add hook
xhook::hooker::add</* replacement function as template parameter */>(/* target function address */);
// Call original function
xhook::hooker::call</* replacement function as template parameter */>(...);
// Remove hook
xhook::hooker::del</* replacement function as template parameter */>();
```

```cpp
#include <iostream>
#include <xhook.hpp>

static __declspec(noinline) auto __fastcall test(int v1, int v2, int v3) -> int
{
	int result = v1 + v2 + v3;
	std::cout << "test: " << result << std::endl;
	return result;
}

static __declspec(noinline) auto __fastcall test_hook(int v1, int v2, int v3) -> int
{
	std::cout << "call test_hook" << std::endl;
	return xhook::hooker::call<test_hook>(v1, v2, v3); // Call original function
}

auto main(int, char**) -> int
{
	xhook::hooker::add<test_hook>(test); // Add hook

	// Test
	test(11, 45, 14);

    xhook::hooker::del<test_hook>(); // Remove hook
	return {};
}
```

### HOOK class member functions (thiscall)

```cpp
#include <iostream>
#include <xhook.hpp>

class my_object
{
public:
	my_object() = default;
	~my_object() = default;
	int m_value = 0;
	std::string m_str = "hello world!";

	__declspec(noinline) auto test1(int value1, int value2)  -> void
	{
		this->m_value = value1 + value2;
		std::cout <<"call my_object::test1" << std::endl;
	}

	__declspec(noinline) auto test2(int value1, int value2) -> void
	{
		this->m_value = value1 * value2;
		std::cout <<"call my_object::test2" << std::endl;
	}
};

struct my_object2
{
	int m_value;
	std::string m_str;

	__declspec(noinline) auto test1(int value1, int value2) -> void
	{
		std::cout << "call my_object2::test1\n" << std::hex;
		std::cout << "[hooked] value1:  0x" << value1 << std::endl;
		std::cout << "[hooked] value2:  0x" << value2 << std::endl;
		std::cout << "[hooked] m_value: 0x" << this->m_value << std::endl;
		std::cout << "[hooked] m_str: " << this->m_str << std::endl;

		// Call original function
		(this->*xhook::hooker::call<&my_object2::test1>)(value2, value2);
	}

	__declspec(noinline) auto test2(int value1, int value2) -> void
	{
		std::cout << "call my_object2::test2\n" << std::hex;
		std::cout << "[hooked] value1:  0x" << value1 << std::endl;
		std::cout << "[hooked] value2:  0x" << value2 << std::endl;
		std::cout << "[hooked] m_value: 0x" << this->m_value << std::endl;
		std::cout << "[hooked] m_str: " << this->m_str << std::endl;

		// Call original function
		(this->*xhook::target<&my_object2::test2>::call)(value2, value2);
	}

};

auto main(int, char**) -> int
{
    // Add hook
	xhook::ptr{ &my_object::test1 } ->* xhook::target<&my_object2::test1>{};
	xhook::hooker::add<&my_object::test2>(&my_object2::test2);

	// Test
	my_object object{};
	object.test1(0x1918, 0x01);
	object.m_str = "u'r the dragon?";
	object.test2(0x114510, 0x04);

    // Remove hook
    xhook::target<&my_object2::test1>::unhook();
	xhook::hooker::del<&my_object::test2>();

	return {};
}
```

## `xhook::add_hook`

```cpp
xhook::add_hook(/* target function address */, /* lambda callback */);
```

```cpp
#include <iostream>
#include <xhook.hpp>

static __declspec(noinline) auto __fastcall test(const char* str1, const char* str2) -> int
{
	std::cout << str1 << str2 << std::endl;
	return 0x114514;
}

xhook::unhooker g_unhooker;

auto main(int, char**) -> int
{
    // Add hook
	auto result = xhook::add_hook(test, xhook::fastcall
	{
		[]<xhook::backup auto backup>
		(const char* str1, const char* str2) -> int
		{
			std::cout << "[hooked] " << str1 << std::endl;
			std::cout << "[hooked] " << str2 << std::endl;
			return backup(str1, str2); // Call original function
		}
	});

	test("hello", "world"); // Test

    // Directly remove hook
    (*result).unhook();

	// Save to global variable, then remove
    g_unhooker = **result;
    g_unhooker.unhook();

	return {};
}
```

If you don't need to call the original function, you can omit `xhook::backup auto backup`

```cpp
auto result = xhook::add_hook(test, xhook::fastcall
{
	[](const char* str1, const char* str2) -> int
	{
		std::cout << "[hooked] " << str1 << std::endl;
		std::cout << "[hooked] " << str2 << std::endl;
		return {};
	}
});
```

HOOK class member functions (thiscall)

```cpp
#include <iostream>
#include <xhook.hpp>

class my_object
{
public:
	my_object() = default;
	~my_object() = default;
	int m_value = 0;
	std::string m_str = "hello world!";

	__declspec(noinline) auto test(int value1, int value2)  -> void
	{
		this->m_value = value1 + value2;
		std::cout <<"call my_object::test" << std::endl;
	}
};

auto main(int, char**) -> int
{
	auto result = xhook::add_hook(&my_object::test, xhook::thiscall
	{
		[]<xhook::backup auto backup>
		(my_object* m_this, int value1, int value2) -> void
		{
			std::cout << "[hooked] value1:  0x" << std::hex << value1 << std::endl;
			std::cout << "[hooked] value2:  0x" << std::hex << value2 << std::endl;
			std::cout << "[hooked] m_value: 0x" << m_this->m_value << std::endl;
			std::cout << "[hooked] m_str: " << m_this->m_str << std::endl;
			backup(m_this, value1, value2);
		}
	});

	// Test
	my_object object{};
	object.test(0x1918, 0x01);
	object.m_str = "u'r the dragon?";
	object.test(0x114510, 0x04);

    (*result).unhook();
	return {};
}
```

## `xhook::dll` 、`xhook::sym`、`xhook::xmodule`、`xhook::symbol`、`xhook::symstr`

```cpp
#include <iostream>
#include <windows.h>
#include <xhook.hpp>

auto main(int, char**) -> int
{
	using namespace xhook::sym;
	using namespace xhook::dll;

	const auto user32_dll = L"User32"_dll; // Get module

	// Equivalent to
	//xhook::xmodule user32_dll = L"User32.dll";

	// Add hook via `+=` operator
	user32_dll += "MessageBoxExW"_sym ->* xhook::winapi
	{
		[]<xhook::backup auto backup>
		(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId) static -> int
		{
			std::wstring msg = L"[hooked] ";
			return backup(hWnd, msg.append(lpText).data(), lpCaption, uType, wLanguageId);
		}
	};

	// Add hook via add_hook function (obviously)
	auto orgMessageBoxW = user32_dll.add_hook
	(
		// xhook::symstr supports dynamic strings, e.g. std::string
		xhook::symstr{ "MessageBoxW" } ->* xhook::winapi
		{
			[]<xhook::backup auto backup>
			(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) static -> int
			{
				std::wstring msg = L"[hooked] ";
				return backup(hWnd, msg.append(lpText).data(), lpCaption, uType);
			}
		}
	);

	// Add hook via `&=` operator. Equivalent to `user32_dll.add_hook(xhook::symbol<>{} ->* hook{})`
	auto orgMessageBoxA = user32_dll &= xhook::symbol<"MessageBoxA">{} ->* xhook::winapi
	{
		[]<xhook::backup auto backup>
		(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) static -> int
		{
			std::string msg = "[hooked] ";
			return backup(hWnd, msg.append(lpText).data(), lpCaption, uType);
		}
	};

	// Test
	::MessageBoxA  (NULL,  "Hello Neko",  "Nekonya~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxW  (NULL, L"Neko World",  L"Nyanko~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxExW(NULL, L"Hello World", L"Nya~~~~",  MB_YESNO | MB_ICONQUESTION,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
	);

	return {};
}
```

### Adding hooks without lambda expressions

```cpp
#include <iostream>
#include <windows.h>
#include <xhook.hpp>

static auto WINAPI MessageBoxA_Hook(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) -> int
{
	std::string msg = "[hooked] ";
	return xhook::target<MessageBoxA_Hook>::call(hWnd, msg.append(lpText).data(), lpCaption,  uType);
}

static auto WINAPI MessageBoxW_Hook(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int
{
	std::wstring msg = L"[hooked] ";
	return xhook::target<MessageBoxW_Hook>::call(hWnd, msg.append(lpText).data(), lpCaption, uType);
}

static auto WINAPI MessageBoxExW_Hook(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId) -> int
{
	std::wstring msg = L"[hooked] ";
	return xhook::target<MessageBoxExW_Hook>::call(hWnd, msg.append(lpText).data(), lpCaption, uType, wLanguageId);
}

auto main(int, char**) -> int
{
	using namespace xhook::sym;
	using namespace xhook::dll;

	const auto user32_dll = L"User32"_dll;

	// Use symbol ->* target form
	user32_dll += xhook::symbol<"MessageBoxExW">{} ->* xhook::target<MessageBoxExW_Hook>{};;
	auto orgMessageBoxA = user32_dll.add_hook("MessageBoxA"_sym ->* xhook::target<MessageBoxA_Hook>{});
	auto orgMessageBoxW = user32_dll &= xhook::symstr{ "MessageBoxW" } ->* xhook::target<MessageBoxW_Hook>{};

	// Or use this way
	// user32_dll.add_hook<MessageBoxA_Hook>("MessageBoxA");
	// user32_dll.add_hook<MessageBoxW_Hook>("MessageBoxW");
	// user32_dll.add_hook<MessageBoxExW_Hook>("MessageBoxExW");

	// Test
	::MessageBoxA  (NULL,  "Hello Neko",  "Nekonya~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxW  (NULL, L"Neko World",  L"Nyanko~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxExW(NULL, L"Hello World", L"Nya~~~~",  MB_YESNO | MB_ICONQUESTION,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
	);

	return {};
}
```

# `xhook::hooks`、`xhook::call` Batch add hooks

```cpp
#include <iostream>
#include <print>
#include <windows.h>
#include <xhook.hpp>
using namespace xhook::call;

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

	__declspec(noinline) auto my_method_hook(int a, int b) noexcept -> int
	{
		this->value = a * b;
		std::println("[my_hooks::my_method_hook] value: {} a: {:02} b: {:02}", this->value, a, b);

		return hooks->*call<&my_hooks::my_method_hook>(this, 11, 45);
	}

	static inline xhook::hooks hooks
	{
		xhook::ptr{ ::MessageBoxA } && xhook::target<my_hooks::MessageBoxA>{},
		xhook::ptr{ ::MessageBoxW } && xhook::target<my_hooks::MessageBoxW>{},
		xhook::target<&my_hooks::my_method_hook>{}
	};

	int value;
};

class my_class
{
public:

	int value;

	__declspec(noinline) auto my_method(int a, int b) noexcept -> int
	{
		this->value += a + b;
		std::println("[my_class::my_method] value: {} a: {:02} b: {:02}", this->value, a, b);
		return this->value;
	}

};

extern "C" auto main(int, char**) -> int
{
	my_hooks::hooks.set<&my_hooks::my_method_hook>(&my_class::my_method);
	my_hooks::hooks.commit();

	my_class obj{ 114 };
	obj.my_method(11, 22);

	::MessageBoxA(NULL,  "MessageBoxA\nHello World!",  "Test example11", MB_YESNO | MB_ICONQUESTION);
	::MessageBoxW(NULL, L"MessageBoxW\nHello World!", L"Test example11", MB_YESNO | MB_ICONQUESTION);

	return {};
}
```

For other demo code, please see [example](https://github.com/cokkeijigen/xhook/tree/master/example).

Some abstract syntax is borrowed from [lsplant::hook_helper](https://github.com/LSPosed/LSPlant/blob/master/lsplant/src/main/jni/include/utils/hook_helper.hpp). <br>

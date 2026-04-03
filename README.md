# XHook - 一个基于现代CPP与Detours封装的~~小玩具~~（库）
<p align="center"> 
	<a href="https://github.com/cokkeijigen/xhook/blob/master/README.md">简体中文</a> | 
	<a href="https://github.com/cokkeijigen/xhook/blob/master/README_EN.md">English</a> <br>
	孩子不懂事写着玩的，<del>反正也没人会用我写的垃圾玩意（bushi）</del><br>
</p>

![](https://github.com/cokkeijigen/xhook/blob/master/image.jpg?raw=true) <br>
**※当前项目仍处于开发阶段，之后的更新可能存在毁灭性的改变，不建议使用到正式项目中。**
# 如何使用（需要cmake和vcpkg）
在你的项目的vcpkg.json中dependencies加入detours
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
在`CMakeLists.txt`中添加
```cmake
set(CMAKE_CXX_STANDARD 23) #建议使用cpp23

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
或者使用`git clone -b master https://github.com/cokkeijigen/xhook.git` 克隆到你的项目文件夹下，再`add_subdirectory(xhook)`也行。

## `xhook::ptr ->* xhook::target` 与 `xhook::target::call`
```cpp
// 添加hook
xhook::ptr{ /* 目标函数地址 */ } ->* xhook::target</* 将替换的函数作为模板参数 */>{};

// 调用原函数
xhook::target</* 将替换的函数作为模板参数 */>::call(.....);
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
	return xhook::target<test_hook>::call(v1, v2, v3); // 调用原函数
}

auto main(int, char**) -> int
{
    // 添加hook
	xhook::ptr{ test } ->* xhook::target<test_hook>{}; 

	// 测试
	test(11, 45, 14);
    
    //删除hook
    xhook::target<test_hook>::unhook();
	return {};
}
```

## `xhook::hooker::add_hook` 与 `xhook::hooker::call`
```cpp
// 添加hook
xhook::hooker::add</* 将替换的函数作为模板参数 */>(/* 目标函数地址 */);
// 调用原函数
xhook::hooker::call</* 将替换的函数作为模板参数 */>(...);
// 删除hook
xhook::hooker::del</* 将替换的函数作为模板参数 */>();
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
	return xhook::hooker::call<test_hook>(v1, v2, v3); // 调用原函数
}

auto main(int, char**) -> int
{
	xhook::hooker::add<test_hook>(test); // 添加hook

	// 测试
	test(11, 45, 14);

    xhook::hooker::del<test_hook>(); // 删除hook
	return {};
}
```
### HOOK类成员函数（thiscall）
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
		
		// 调用原函数
		(this->*xhook::hooker::call<&my_object2::test1>)(value2, value2);
	}

	__declspec(noinline) auto test2(int value1, int value2) -> void
	{
		std::cout << "call my_object2::test2\n" << std::hex;
		std::cout << "[hooked] value1:  0x" << value1 << std::endl;
		std::cout << "[hooked] value2:  0x" << value2 << std::endl;
		std::cout << "[hooked] m_value: 0x" << this->m_value << std::endl;
		std::cout << "[hooked] m_str: " << this->m_str << std::endl;
		
		// 调用原函数
		(this->*xhook::target<&my_object2::test2>::call)(value2, value2);
	}
	
};

auto main(int, char**) -> int
{
    // 添加hook
	xhook::ptr{ &my_object::test1 } ->* xhook::target<&my_object2::test1>{};
	xhook::hooker::add<&my_object::test2>(&my_object2::test2);

	// 测试
	my_object object{};
	object.test1(0x1918, 0x01);
	object.m_str = "u'r the dragon?";
	object.test2(0x114510, 0x04);
    
    //删除hook
    xhook::target<&my_object2::test1>::unhook();
	xhook::hooker::del<&my_object::test2>();
    
	return {};
}
```
## `xhook::add_hook`
```cpp
xhook::add_hook(/* 目标函数地址 */, /* lambda回调 */);
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
    // 添加hook
	auto result = xhook::add_hook(test, xhook::fastcall
	{
		[]<xhook::backup auto backup>
		(const char* str1, const char* str2) -> int
		{
			std::cout << "[hooked] " << str1 << std::endl;
			std::cout << "[hooked] " << str2 << std::endl;
			return backup(str1, str2); // 调用原函数
		}
	});

	test("hello", "world"); // 测试

    // 直接解除hook
    (*result).unhook();
    
	// 保存到全局变量中，再解除
    g_unhooker = **result;
    g_unhooker.unhook();
    
	return {};
}
```
如果不需要调用原来的函数，可以省略`xhook::backup auto backup`
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
HOOK类成员函数（thiscall）
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
    
	// 测试
	my_object object{};
	object.test(0x1918, 0x01);
	object.m_str = "u'r the dragon?";
	object.test(0x114510, 0x04);
	
    （*result).unhook();
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

	const auto user32_dll = L"User32"_dll; // 获取模块
	
	// 等同于
	//xhook::xmodule user32_dll = L"User32.dll";
	
	// 通过`+=`运算符来添加hook
	user32_dll += "MessageBoxExW"_sym ->* xhook::winapi
	{
		[]<xhook::backup auto backup>
		(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId) static -> int
		{
			std::wstring msg = L"[hooked] ";
			return backup(hWnd, msg.append(lpText).data(), lpCaption, uType, wLanguageId);
		}
	};

	// 通过add_hook函数添加hook（废话）
	auto orgMessageBoxW = user32_dll.add_hook
	(
		// xhook::symstr支持动态字符串，例如std::string
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

	// 通过`&=`运算符添加hook。 相当于`user32_dll.add_hook(xhook::symbol<>{} ->* hook{})`
	auto orgMessageBoxA = user32_dll &= xhook::symbol<"MessageBoxA">{} ->* xhook::winapi
	{
		[]<xhook::backup auto backup>
		(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) static -> int
		{
			std::string msg = "[hooked] ";
			return backup(hWnd, msg.append(lpText).data(), lpCaption, uType);
		}
	};

	// 测试
	::MessageBoxA  (NULL,  "Hello Neko",  "Nekonya~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxW  (NULL, L"Neko World",  L"Nyanko~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxExW(NULL, L"Hello World", L"Nya~~~~",  MB_YESNO | MB_ICONQUESTION, 
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
	);

	return {};
}
```
### 不使用lambda表达式添加hook
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

	// 使用 symbol ->* target的形式
	user32_dll += xhook::symbol<"MessageBoxExW">{} ->* xhook::target<MessageBoxExW_Hook>{};;
	auto orgMessageBoxA = user32_dll.add_hook("MessageBoxA"_sym ->* xhook::target<MessageBoxA_Hook>{});
	auto orgMessageBoxW = user32_dll &= xhook::symstr{ "MessageBoxW" } ->* xhook::target<MessageBoxW_Hook>{};

	// 也可通过这种方式
	// auto orgMessageBoxA   = user32_dll.add_hook<MessageBoxA_Hook>("MessageBoxA");
	// auto orgMessageBoxW   = user32_dll.add_hook<MessageBoxW_Hook>("MessageBoxW");
	// auto orgMessageBoxExW = user32_dll.add_hook<MessageBoxExW_Hook>("MessageBoxExW");

	// 测试
	::MessageBoxA  (NULL,  "Hello Neko",  "Nekonya~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxW  (NULL, L"Neko World",  L"Nyanko~",  MB_YESNO | MB_ICONQUESTION);
	::MessageBoxExW(NULL, L"Hello World", L"Nya~~~~",  MB_YESNO | MB_ICONQUESTION, 
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
	);

	return {};
}
```
# `xhook::hooks`、`xhook::call` 批量添加hook
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
其他演示代码请看[example](https://github.com/cokkeijigen/xhook/tree/master/example)。

部分抽象写法借鉴了[lsplant::hook_helper](https://github.com/LSPosed/LSPlant/blob/master/lsplant/src/main/jni/include/utils/hook_helper.hpp)。 <br>

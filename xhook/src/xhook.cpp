#include <iostream>
#include <xhook.hpp>
#include <windows.h>
#include <detours.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define noinline __declspec(noinline)
#else
#define noinline __attribute__((noinline))
#endif

namespace xhook
{

    auto init(void) noexcept -> void
    {
        ::DetourRestoreAfterWith();
    }

    auto commit(void) noexcept -> bool
    {
        return ::DetourTransactionCommit() == NO_ERROR;
    }

    static noinline auto hook(void** raw, void* target, bool unhook, bool commit) noexcept -> bool
    {
        ::DetourTransactionBegin();
        ::DetourUpdateThread(::GetCurrentThread());

        LONG status
        { 
            unhook ?
            ::DetourDetach(raw, target) :
            ::DetourAttach(raw, target)
        };
        
        if (commit)
        {
            status = ::DetourTransactionCommit();
        }
        return status == NO_ERROR;
    }

    static noinline auto hooks(void** raws, void** targets, size_t count, bool unhook, bool commit) noexcept -> bool
    {
        if (count > 0)
        {
            ::DetourTransactionBegin();
            ::DetourUpdateThread(::GetCurrentThread());

            auto attach_detach{ unhook ? ::DetourDetach : ::DetourAttach };

            for (size_t i{}; i < count; i++)
            {
                attach_detach(&raws[i], targets[i]);
            }

            if (commit)
            {
                const LONG status{ ::DetourTransactionCommit() };
                return status == NO_ERROR;
            }
            return true;
        }
        return false;
    }


    auto add_hook(void** raw, void* target, bool commit) noexcept -> bool
    {
        return xhook::hook(raw, target, false, commit);
    }
    
    auto add_hooks(void** raws, void** targets, size_t count, bool commit) noexcept -> bool
    {
        return xhook::hooks(raws, targets, count, false, commit);
    }

    auto unhook(void** raw, void* target, bool commit) noexcept -> bool
    {
        return xhook::hook(raw, target, true, commit);
    }

    auto xhook::unhooks(void** raws, void** targets, size_t count, bool commit) noexcept -> bool
    {
        return xhook::hooks(raws, targets, count, true, commit);
    }

    auto get_base(void) noexcept -> void*
    {
        return ::GetModuleHandleW(NULL);
    }

    auto get_base(const char* module) noexcept -> void*
    {
        return ::GetModuleHandleA(module);
    }

    auto get_base(const wchar_t* module) noexcept -> void*
    {
        return ::GetModuleHandleW(module);
    }

    auto get_addr(void* const base, const char* symbol) noexcept -> void*
    {
        return ::GetProcAddress(reinterpret_cast<HMODULE>(base), symbol);
    }

    auto get_addr(const char* symbol) noexcept -> void*
    {
        return ::GetProcAddress(::GetModuleHandleW(NULL), symbol);
    }
}

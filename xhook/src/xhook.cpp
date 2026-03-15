#include <iostream>
#include <xhook.hpp>
#include <windows.h>
#include <detours.h>

namespace xhook
{
    auto add_hook(void* raw, void* target) noexcept -> void*
    {
        ::DetourRestoreAfterWith();
        ::DetourTransactionBegin();
        ::DetourUpdateThread(GetCurrentThread());
        ::DetourAttach(&raw, target);
        ::DetourTransactionCommit();
        return raw;
    }
   
}
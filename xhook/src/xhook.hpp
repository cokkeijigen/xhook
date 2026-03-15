#pragma once

namespace xhook
{

    template <typename caller>
    struct backup_context 
    {
        constexpr backup_context() noexcept = default;
        
        template<typename... args>
        auto operator()(args&&... _args) const noexcept
        {
            return caller::raw(std::forward<args>(_args)...);
        }
    };

    template <typename T>
    struct is_backup_context : std::false_type {};

    template <typename caller>
    struct is_backup_context<backup_context<caller>> : std::true_type {};

    template<typename T>
    concept backup = is_backup_context<T>::value;

    template<typename T, typename = void>
    struct is_simple_lambda : std::false_type {};

    template<typename T>
    struct is_simple_lambda<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

    template<typename T, typename caller, typename = void>
    struct is_template_backup_lambda : std::false_type {};

    template<typename T, typename caller>
    struct is_template_backup_lambda <T, caller, 
        std::void_t<decltype(&T::template operator()<backup_context<caller>{}>)>
    > : std::true_type {};

    template <typename traits, typename caller, typename lambda>
    class function
    {
        alignas(lambda) 
        static inline uint8_t data[sizeof(lambda)];

        static inline           bool initialized{ false };
        static inline constexpr bool is_stateless
        {
            std::is_empty_v<lambda> &&
            std::is_default_constructible_v<lambda>
        };

    public:

        using funcptr_t = typename traits::pointer;
        static inline funcptr_t raw{};

        inline function(lambda _lambda) noexcept
        {
            if constexpr (!is_stateless)
            {
                if (initialized)
                {
                    reinterpret_cast<lambda*>(data)->~lambda();
                }
                new (data) lambda(std::move(_lambda));
                initialized = true;
            }
        }

        template<typename... args>
        static auto operator()(args... _args) noexcept -> typename traits::ret_type
        {
            if constexpr (is_simple_lambda<lambda>::value)
            {
                if constexpr (is_stateless)
                {
                    return lambda{}(std::forward<args>(_args)...);
                }
                else
                {
                    return (*reinterpret_cast<lambda*>(data))(std::forward<args>(_args)...);
                }
            }
            else
            {
                if constexpr (is_stateless)
                {
                    return lambda{}.template 
                           operator()<backup_context<caller>{}>(std::forward<args>(_args)...);
                }
                else
                {
                    return (*reinterpret_cast<lambda*>(data)).template
                           operator()<backup_context<caller>{}>(std::forward<args>(_args)...);
                }
            }
        }

        static auto get() noexcept -> funcptr_t
        {
            return static_cast<funcptr_t>(caller::operator());
        }
    };

    template <typename T>
    concept is_function = requires
    {
        { T::raw   } -> std::convertible_to<void*>;
        { T::get() } -> std::convertible_to<void*>;
    };

    #define xhook_declare_caller(name, caller)/******************************************************************/\
    /**/   template <typename lambda>                                                                             \
    /**/   struct name;                                                                                           \
    /**/                                                                                                          \
    /**/   template <typename T, typename = void>                                                                 \
    /**/   struct name##_lambda_traits;                                                                           \
    /**/                                                                                                          \
    /**/   template <typename ret, typename... args>                                                              \
    /**/   struct name##_lambda_traits<ret(*)(args...)>                                                           \
    /**/   {                                                                                                      \
    /**/       using pointer  = ret(caller*)(args...);                                                            \
    /**/       using ret_type = ret;                                                                              \
    /**/   };                                                                                                     \
    /**/                                                                                                          \
    /**/   template <typename clazz, typename ret, typename... args>                                              \
    /**/   struct name##_lambda_traits<ret(clazz::*)(args...) const>                                              \
    /**/   {                                                                                                      \
    /**/       using pointer  = ret(caller*)(args...);                                                            \
    /**/       using ret_type = ret;                                                                              \
    /**/   };                                                                                                     \
    /**/                                                                                                          \
    /**/   template <typename clazz, typename ret, typename... args>                                              \
    /**/   struct name##_lambda_traits<ret(clazz::*)(args...)>                                                    \
    /**/   {                                                                                                      \
    /**/       using pointer  = ret(caller*)(args...);                                                            \
    /**/       using ret_type = ret;                                                                              \
    /**/   };                                                                                                     \
    /**/                                                                                                          \
    /**/   template <typename T>                                                                                  \
    /**/   struct name##_lambda_traits<T, std::enable_if_t<xhook::is_simple_lambda<T>::value>>                    \
    /**/       : name##_lambda_traits<decltype(&T::operator())> {};                                               \
    /**/                                                                                                          \
    /**/   template <typename T>                                                                                  \
    /**/   struct name##_lambda_traits<T, std::enable_if_t<xhook::is_template_backup_lambda<T, name<T>>::value>>  \
    /**/       : name##_lambda_traits<decltype(&T::template operator()<xhook::backup_context<name<T>>{}>)> {};    \
    /**/                                                                                                          \
    /**/   template <typename lambda>                                                                             \
    /**/   struct name : public xhook::function<name##_lambda_traits<lambda>, name<lambda>, lambda>               \
    /**/   {                                                                                                      \
    /**/       using    traits = name##_lambda_traits<lambda>;                                                    \
    /**/       using  function = xhook::function<name##_lambda_traits<lambda>, name<lambda>, lambda>;             \
    /**/       inline name(lambda _lambda) noexcept : function{ _lambda } {}                                      \
    /**/                                                                                                          \
    /**/       template<typename... args>                                                                         \
    /**/       static auto caller operator()(args... _args) noexcept -> typename traits::ret_type                 \
    /**/       {                                                                                                  \
    /**/           return function::operator()(std::forward<args>(_args)...);                                     \
    /**/       }                                                                                                  \
    /**/   };                                                                                                     \
    /**/                                                                                                          \
    /**[xhook_declare_caller]*************************************************************************************/

    extern auto add_hook(void* raw, void* target) noexcept -> void*;

    template<is_function func>
    static inline auto add_hook(void* raw, func) noexcept -> void*
    {
        void* const ptr{ xhook::add_hook(raw, func::get()) };
        func::raw = reinterpret_cast<decltype(func::raw)>(ptr);
        return ptr;
    }

    xhook_declare_caller(cdecl_t , __cdecl   );
    xhook_declare_caller(stdcall , __stdcall );
    xhook_declare_caller(fastcall, __fastcall);
    
    #ifdef WINAPI
    xhook_declare_caller(winapi, WINAPI);
    #endif

    #ifdef _WINDOWS_
    
    template <typename sym_t, typename func_t>
    struct entry
    {
        sym_t &&   symbol;
        func_t&& function;

        constexpr entry(sym_t&& _symbol, func_t&& _function) noexcept :
              symbol{ std::forward<sym_t> (_symbol)   },
            function{ std::forward<func_t>(_function) }
        {
        }
    };

    template <typename T>
    concept is_hook_entry = requires(T _entry)
    {
        _entry.symbol.data;
        xhook::add_hook((void*)(nullptr), _entry.function);
    };

    struct xmodule
    {
        const HMODULE handle;

        inline xmodule() noexcept : handle{ ::GetModuleHandleA(nullptr) } 
        {
        };

        inline xmodule(const char*    name) noexcept: handle{ ::GetModuleHandleA(name) }
        {
        }

        inline xmodule(const wchar_t* name) noexcept: handle{ ::GetModuleHandleW(name) }
        {
        }

        inline xmodule(HMODULE _handle) noexcept : handle{ _handle }
        {
        }

        template <class symstr_t, is_function func_t>
        requires is_hook_entry<entry<symstr_t, func_t>>
        inline auto add_hook(entry<symstr_t, func_t>&& _entry) const noexcept -> typename func_t::funcptr_t
        {
            if (this->handle != nullptr)
            {
                void* const ptr{ ::GetProcAddress(this->handle, _entry.symbol.data) };
                if (ptr != nullptr)
                {
                    auto result{ xhook::add_hook(ptr, _entry.function) };
                    return reinterpret_cast<typename func_t::funcptr_t>(result);
                }
            }
            return nullptr;
        }
        
        template <class symstr_t,   is_function func_t>
        requires is_hook_entry<entry<symstr_t, func_t>>
        auto operator&(entry<symstr_t, func_t>&& _entry) const noexcept -> typename func_t::funcptr_t
        {
            return this->add_hook(std::move(_entry));
        }

        template <class symstr_t,   is_function func_t>
        requires is_hook_entry<entry<symstr_t, func_t>>
        auto operator+=(entry<symstr_t, func_t>&& _entry) noexcept -> xmodule&
        {
            this->add_hook(std::move(_entry));
            return *this;
        }

        template <class symstr_t,   is_function func_t>
        requires is_hook_entry<entry<symstr_t, func_t>>
        auto operator+=(entry<symstr_t, func_t>&& _entry) const noexcept -> const xmodule&
        {
            this->add_hook(std::move(_entry));
            return *this;
        }
    };

    template <size_t N>
    struct string_symbol
    {
        char data[N]{};
        consteval string_symbol(const char(&str)[N]) noexcept
        {
            std::copy_n(str, N, data);
        }
    };

    template <class char_t, size_t N>
    struct string_dll
    {
        char_t data[N + 4]{};
        consteval string_dll(const char_t(&str)[N]) noexcept
        {
            std::copy_n(str, N, data);
            data[N - 1] = static_cast<char_t>('.' );
            data[N + 0] = static_cast<char_t>('d' );
            data[N + 1] = static_cast<char_t>('l' );
            data[N + 2] = static_cast<char_t>('l' );
            data[N + 3] = static_cast<char_t>('\0');
        }
    };

    template <string_symbol str>
    struct symbol
    {
        template <is_function func_t, class symstr_t = const decltype(str)>
        inline auto operator->*(func_t&& func) noexcept -> entry<symstr_t, func_t>
        {
            return { std::move(str), std::forward<func_t>(func) };
        }
    };

    namespace sym
    {
        template <string_symbol _symbol>
        constexpr auto operator""_sym() noexcept -> symbol<_symbol>
        {
            return {};
        }
    }

    namespace dll
    {
        template <string_dll _dll>
        constexpr auto operator""_dll() noexcept -> xmodule
        {
            return xmodule{ _dll.data };
        }
    }

    #endif
}

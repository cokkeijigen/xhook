#pragma once
#include <type_traits>
#include <functional>
#include <tuple>

namespace xhook
{

    template <class caller>
    struct backup_context 
    {
        constexpr backup_context() noexcept = default;
        
        template<class... args>
        auto operator()(args&&... _args) const noexcept
        {
            return caller::raw(std::forward<args>(_args)...);
        }
    };

    template <class T>
    struct is_backup_context : std::false_type {};

    template <class caller>
    struct is_backup_context<backup_context<caller>> : std::true_type {};

    template<class T>
    concept backup = is_backup_context<T>::value;

    template<class T, class = void>
    struct is_simple_lambda : std::false_type {};

    template<class T>
    struct is_simple_lambda<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

    template<class T, class caller, class = void>
    struct is_template_backup_lambda : std::false_type {};

    template<class T, class caller>
    struct is_template_backup_lambda <T, caller, 
        std::void_t<decltype(&T::template operator()<backup_context<caller>{}>)>
    > : std::true_type {};

    template<class T>
    concept is_xhook_function = requires
    {
        *reinterpret_cast<void**>(&T::raw) = nullptr;
        { T::raw   } -> std::convertible_to<void*>;
        { T::get() } -> std::convertible_to<void*>;
    };
     
    template<class T>
    concept is_function_pointer = std::is_member_function_pointer_v<std::remove_reference_t<T>> ||
                                  std::is_function_v<std::remove_pointer_t<std::remove_reference_t<T>>>;

    template<class T>
    requires xhook::is_function_pointer<T>
    union unifptr_t
    {
        T raw;
        void* ptr;

        inline unifptr_t(T _raw) : raw{ _raw }
        {
        }
    };

    template <class traits, class caller, class lambda>
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

        inline function() noexcept {};

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

        template<class... args>
        static auto operator()(args... _args) noexcept -> typename traits::ret_type
        {
            if constexpr (xhook::is_simple_lambda<lambda>::value)
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
                           operator()<xhook::backup_context<caller>{}>(std::forward<args>(_args)...);
                }
                else
                {
                    return (*reinterpret_cast<lambda*>(data)).template
                           operator()<xhook::backup_context<caller>{}>(std::forward<args>(_args)...);
                }
            }
        }

        static auto get() noexcept -> funcptr_t
        {
            return static_cast<funcptr_t>(caller::operator());
        }
    };

    extern auto  begin_hook() noexcept -> void;
    extern auto commit_hook() noexcept -> bool;

    extern auto add_hook (void**  raw, void*  target) noexcept -> void;
    extern auto add_hook (void*   raw, void*  target) noexcept -> void*;
    extern auto add_hooks(void** raws, void** targets, size_t count) noexcept -> bool;
    
    extern auto unhook (void* raw, void* target) noexcept -> void;
    extern auto unhooks(void** raws, void** targets, size_t count) noexcept -> void;

    extern auto get_base() noexcept -> void*;
    extern auto get_base(const char*    module) noexcept -> void*;
    extern auto get_base(const wchar_t* module) noexcept -> void*;

    extern auto get_addr(const char* symbol) noexcept -> void*;
    extern auto get_addr(void* const base, const char* symbol) noexcept -> void*;

    class unhooker
    {
        void* const m_raw;
        void* const m_tar;
        const uint8_t m_type;

    public:

        unhooker() = default;

        inline unhooker(void* const raw, void* const tar, uint8_t type)
            noexcept : m_raw{ raw }, m_tar{ tar }, m_type{ type }
        {
        }

        inline auto raw() const noexcept -> void* 
        {
            if (this->m_raw != nullptr && (this->m_type & 0b01))
            {
                return *reinterpret_cast<void**>(this->m_raw);
            }
            else 
            {
                return this->m_raw;
            }
        }

        inline auto tar() const noexcept -> void*
        {
            if (this->m_tar != nullptr && (this->m_type & 0b10))
            {
                return *reinterpret_cast<void**>(this->m_tar);
            }
            else 
            {
                return this->m_tar;
            }
        }

        inline auto unhook() const noexcept -> void
        {
            xhook::unhook(this->raw(), this->tar());
        };
    };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct result
    {
        struct entry
        {
            const raw_t raw;
            const tar_t tar;
            
            inline entry(raw_t _raw, tar_t _tar) noexcept
                : raw{ _raw }, tar{ _tar }
            {
            }
        private:
            const uint8_t type
            {
                (std::is_reference_v<raw_t> ? 0b01 : 0) |
                (std::is_reference_v<tar_t> ? 0b10 : 0)
            };
        };
        
        union uniondata
        {
            entry entry;
            xhook::unhooker unhooker;
        };

        const uniondata data;

        inline result(raw_t _raw, tar_t _tar) noexcept : data{ .entry{ _raw, _tar } }
        {
        }

        inline auto operator->() const noexcept -> const entry* 
        {
            return &this->data.entry;
        }

        inline auto operator*() const noexcept -> const xhook::unhooker&
        {
            return this->data.unhooker;
        }

        template<std::size_t i>
        requires (i >= 0 && i <= 2)
        auto&& get() const noexcept 
        {
            if constexpr (i == 0)
            {
                return this->data.entry.raw;
            }
            else if constexpr(i == 1)
            {
                return this->data.entry.tar;
            }
            else 
            {
                return this->data.unhooker;
            }
        }
    };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_size<xhook::result<raw_t, tar_t>> : std::integral_constant<size_t, 3> {};

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_element<0, xhook::result<raw_t, tar_t>> { using type = const raw_t; };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_element<1, xhook::result<raw_t, tar_t>> { using type = const tar_t; };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_element<2, xhook::result<raw_t, tar_t>> { using type = const xhook::unhooker; };

    template<class raw_t, class tar_t>
    requires xhook::is_function_pointer<raw_t> && xhook::is_function_pointer<tar_t>
    static inline auto add_hook(raw_t raw, tar_t target) noexcept -> tar_t
    {
        void* ptr{ xhook::add_hook(xhook::unifptr_t{ raw }.ptr, xhook::unifptr_t{ target }.ptr) };
        return *reinterpret_cast<tar_t*>(&ptr);
    }

    template<class raw_t>
    requires xhook::is_function_pointer<raw_t>
    static inline auto add_hook(raw_t raw, void* target) noexcept -> raw_t
    {
        void* ptr{ xhook::add_hook(xhook::unifptr_t{ raw }.ptr, target) };
        return *reinterpret_cast<raw_t*>(&ptr);
    }

    template<class tar_t>
    requires xhook::is_function_pointer<tar_t>
    static inline auto add_hook(void* raw, tar_t target) noexcept -> tar_t
    {
        void* ptr{ xhook::add_hook(raw, xhook::unifptr_t{ target }.ptr) };
        return *reinterpret_cast<tar_t*>(&ptr);
    }

    template<class tar_t>
    requires xhook::is_function_pointer<tar_t>
    static inline auto add_hook(void** raw, tar_t target) noexcept -> void
    {
        xhook::add_hook(raw, xhook::unifptr_t{ target }.ptr);
    }

    template<xhook::is_xhook_function func>
    static inline auto add_hook(void* raw, func) noexcept 
        -> xhook::result<decltype(func::raw)&, decltype(func::raw)>
    {
        *reinterpret_cast<void**>(&func::raw) = raw;
        xhook::add_hook(reinterpret_cast<void**>(&func::raw), func::get());
        return { func::raw, func::get() };
    }

    template<class T, xhook::is_xhook_function func>
    static inline auto add_hook(xhook::unifptr_t<T> raw, func _func) noexcept 
        -> xhook::result<decltype(func::raw)&, decltype(func::raw)>
    {
        return xhook::add_hook(raw.ptr, _func);
    }

	#define xhook_declare_caller(name, caller)/************************************************************************************/\
	/**/	template <class lambda>                                                                                                 \
	/**/	struct name;                                                                                                            \
	/**/																														    \
	/**/	template <class lambda, class = void>                                                                                   \
	/**/	struct name##_lambda_traits;                                                                                            \
	/**/																														    \
	/**/	template <class lambda, class ret, class... args>                                                                       \
	/**/	struct name##_lambda_traits<lambda, ret(*)(args...)>                                                                    \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/			return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda, class ret, class... args>                                                                       \
	/**/	struct name##_lambda_traits<lambda, ret(*)(args...) noexcept>                                                           \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/			return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda, class clazz, class ret, class... args>                                                          \
	/**/	struct name##_lambda_traits<lambda, ret(clazz::*)(args...) const>                                                       \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/            return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/																														    \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda, class clazz, class ret, class... args>                                                          \
	/**/	struct name##_lambda_traits<lambda, ret(clazz::*)(args...) const noexcept>                                              \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/            return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/																														    \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda, class clazz, class ret, class... args>                                                          \
	/**/	struct name##_lambda_traits<lambda, ret(clazz::*)(args...)>                                                             \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/            return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda, class clazz, class ret, class... args>                                                          \
	/**/	struct name##_lambda_traits<lambda, ret(clazz::*)(args...) noexcept>                                                    \
	/**/	{                                                                                                                       \
	/**/		using pointer  = ret(caller*)(args...);                                                                             \
	/**/		using ret_type = ret;                                                                                               \
	/**/																														    \
	/**/		static auto caller operator()(args... value) noexcept -> ret                                                        \
	/**/		{                                                                                                                   \
    /**/            using  function = typename name<lambda>::function;                                                              \
	/**/            return function::operator()(std::forward<args>(value)...);                                                      \
	/**/		}                                                                                                                   \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda>                                                                                                 \
	/**/	struct name##_lambda_traits<lambda, std::enable_if_t<xhook::is_simple_lambda<lambda>::value>>                           \
	/**/		:  name##_lambda_traits<lambda, decltype(&lambda::operator())>                                                      \
	/**/	{                                                                                                                       \
	/**/	};                                                                                                                      \
	/**/																														    \
	/**/	template <class lambda>                                                                                                 \
	/**/	struct name##_lambda_traits<lambda, std::enable_if_t<xhook::is_template_backup_lambda<lambda, name<lambda>>::value>>    \
	/**/		:  name##_lambda_traits<lambda, decltype(&lambda::template operator()<xhook::backup_context<name<lambda>>{}>)>      \
	/**/	{                                                                                                                       \
	/**/	};                                                                                                                      \
    /**/                                                                                                                            \
	/**/	template<class lambda>                                                                                                  \
	/**/	using name##_function = xhook::function<name##_lambda_traits<lambda>, name<lambda>, lambda>;                            \
	/**/																														    \
	/**/	template <class lambda>                                                                                                 \
	/**/	struct name : public name##_function<lambda>, public name##_lambda_traits<lambda>                                       \
	/**/	{                                                                                                                       \
	/**/		using function = name##_function<lambda>;                                                                           \
	/**/																														    \
	/**/		inline name(lambda _lambda) noexcept : function{ _lambda }                                                          \
	/**/		{                                                                                                                   \
	/**/		}                                                                                                                   \
	/**/																														    \
	/**/		using name##_lambda_traits<lambda>::operator();                                                                     \
	/**/	};                                                                                                                      \
	/**[xhook_declare_caller]*******************************************************************************************************/

    xhook_declare_caller(cdecl_t , __cdecl   )
    xhook_declare_caller(stdcall , __stdcall )
    xhook_declare_caller(fastcall, __fastcall)
    xhook_declare_caller(thiscall, __thiscall)

    #ifdef WINAPI
    xhook_declare_caller(winapi, WINAPI)
    #endif

    template<auto _func>
    requires xhook::is_function_pointer<decltype(_func)>
    struct target
    {
        inline           static decltype(_func) call = _func;
        inline constexpr static decltype(_func) func = _func;
    };

    template<class T>
    concept is_hook_target = requires 
    {
        *reinterpret_cast<void**>(&T::call) = nullptr;
        requires xhook::is_function_pointer<decltype(T::call)>;
        requires xhook::is_function_pointer<decltype(T::func)>;
        typename std::integral_constant<decltype(T::func), T::func>;
    };

    template<class T>
    concept has_call_target = requires
    {
        *reinterpret_cast<void**>(&T::call) = nullptr;
        requires xhook::is_function_pointer<decltype(T::call)>;
    };

    template<class T>
    concept has_func_constexpr = requires
    {
        requires xhook::is_function_pointer<decltype(T::func)>;
        typename std::integral_constant<decltype(T::func), T::func>;
    };

    template<class T>
    struct get_function_pointer_type 
    {
        using type = void;
    };

    template<class T>
    requires xhook::is_function_pointer<T>
    struct get_function_pointer_type<T>
    {
        using type = T;
    };

    template<class T>
    requires xhook::is_xhook_function<T>
    struct get_function_pointer_type<T>
    {
        using type = decltype(T::raw);
    };

    template<class T>
    requires xhook::has_func_constexpr<T> || xhook::has_call_target<T>
    struct get_function_pointer_type<T>
    {
    private:

        template<class R>
        struct get_type;

        template<class R>
        requires xhook::is_function_pointer<decltype(T::func)>
        struct get_type<R>
        {
            int _{};
            using type = decltype(T::func);
        };

        template<class R>
        requires xhook::is_function_pointer<decltype(T::call)>
        struct get_type<R>
        {
            long _{};
            using type = decltype(T::call);
        };

    public:

        using type = typename decltype(get_type<T>{0})::type;
    };

    template<class T>
    using get_funcptr = xhook::get_function_pointer_type<T>;

    class hooker 
    {
        static auto add(void** raw, void* target) noexcept -> void;

    public:

        template<auto func>
        requires xhook::is_function_pointer<decltype(func)>
        inline constexpr static decltype(func)& call = xhook::target<func>::call;

        static auto begin () noexcept -> void;

        static auto commit() noexcept -> bool;
       
        template<auto func>
        requires xhook::is_function_pointer<decltype(func)>
        inline static auto add(void* ptr) noexcept -> const decltype(func)&
        {
            *reinterpret_cast<void**>(&xhook::target<func>::call) = ptr;
            xhook::hooker::add
            (   
                reinterpret_cast<void**>(&xhook::target<func>::call),
                xhook::unifptr_t{ func }.ptr
            );
            return xhook::target<func>::call;
        }

        template<auto func, xhook::is_function_pointer T>
        requires xhook::is_function_pointer<decltype(func)>
        inline static auto add(T _ptr) noexcept -> const decltype(func)&
        {
            return xhook::hooker::add<func>(xhook::unifptr_t{ _ptr }.ptr);
        }
       
        template<auto func>
        requires xhook::is_function_pointer<decltype(func)>
        inline static auto add_hook(void* ptr) noexcept -> const decltype(func)&
        {
            void* raw = xhook::add_hook(ptr, xhook::unifptr_t{ func }.ptr);
            *reinterpret_cast<void**>(&xhook::target<func>::call) = raw;
            return xhook::target<func>::call;
        }

        template<auto func, xhook::is_function_pointer T>
        requires xhook::is_function_pointer<decltype(func)>
        inline static auto add_hook(T _ptr) noexcept -> const decltype(func)&
        {
            return xhook::hooker::add_hook<func>(xhook::unifptr_t{ _ptr }.ptr);
        }

        inline  hooker() noexcept { hooker:: begin(); }
        inline ~hooker() noexcept { hooker::commit(); }
    };

    template<auto _func>
    struct empty_entry
    {
        inline constexpr static decltype(_func) func = _func;
    };

    template<auto _func>
    using empety_t = xhook::empty_entry<_func>;

    template <auto _func, class ptr_t = void*, class func_t = xhook::target<_func>>
    requires xhook::is_function_pointer<decltype(_func)>
    struct fixed_function_pointer_entry
    {
        void* ptr{};
       
        using pointer_type  = ptr_t;
        using function_type = func_t;
        
        inline constexpr static decltype(_func) func = _func;

        inline fixed_function_pointer_entry() noexcept {}
        inline fixed_function_pointer_entry(void* _ptr) noexcept : ptr{ _ptr }
        {
        }
    };

    template <auto func, class T>
    using fptrety_t = xhook::fixed_function_pointer_entry<func, T>;

    template<class T>
    concept is_fixed_function_pointer_entry = requires(T entry)
    {
        typename T::pointer_type;
        typename T::function_type;
        requires xhook::has_func_constexpr<T>;
        { entry.ptr } -> std::convertible_to<void*>;
    };

    template<class ptr_t, class func_t>
    concept pointer_entry_types = requires(ptr_t ptr)
    {
        { ptr.value } -> std::convertible_to<void*>;
        requires xhook::is_xhook_function<func_t> || xhook::is_function_pointer<func_t>;
        requires !std::is_same_v<typename xhook::get_funcptr<func_t>::type, void>;
    };

    template<class ptr_t, class func_t>
    requires xhook::pointer_entry_types<ptr_t, func_t>
    struct pointer_entry
    {
        ptr_t &  pointer;
        func_t& function;

        using funcptr_t = typename xhook::get_funcptr<func_t>::type;

        inline constexpr pointer_entry(ptr_t& ptr, func_t& func) noexcept :
            pointer{ ptr }, function{ func }
        {
        }
    };

    template<class ptr_t, class func_t>
    using ptrety_t = xhook::pointer_entry<ptr_t, func_t>;

    template<class T>
    concept is_pointer_entry = requires(T entry)
    {
        typename T::funcptr_t;
        { entry.pointer.value } -> std::convertible_to<void*>;
        { xhook::add_hook((void*)(nullptr), entry.function) } -> std::convertible_to<typename T::funcptr_t>;
    };

    template<class T>
    requires xhook::is_pointer_entry<T>
    struct get_function_pointer_type<T>
    {
        using type = typename T::funcptr_t;
    };

    struct ptr
    {
        void* value;

        inline ptr(void* ptr) noexcept : value{ ptr }
        {
        }

        inline ptr(uintptr_t ptr) noexcept
            : value{ reinterpret_cast<void*>(ptr) }
        {
        }

        template<class T>
        requires xhook::is_function_pointer<T>
        inline ptr(T _ptr) noexcept : value{ xhook::unifptr_t{ _ptr }.ptr }
        {
        }

        template<xhook::has_func_constexpr T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_pointer_entry<T::func, xhook::ptr, T>
        {
            return { this->value };
        }

        template<class T>
        requires xhook::is_hook_target<T> || xhook::has_func_constexpr<T>
        inline auto operator->*(T) noexcept -> decltype(T::func)
        {
            if constexpr (xhook::is_hook_target<T> || xhook::has_call_target<T>)
            {
                if constexpr (xhook::has_func_constexpr<T>) 
                {
                    void* ptr{ xhook::add_hook(this->value, xhook::unifptr_t{ T::func }.ptr) };
                    *reinterpret_cast<void**>(&T::call) = ptr;
                }
                else 
                {
                    void* ptr{ xhook::add_hook(this->value, xhook::unifptr_t{ T::call }.ptr) };
                    *reinterpret_cast<void**>(&T::call) = ptr;
                }
                return T::call;
            }
            else 
            {
                void* result{ xhook::add_hook(this->value, xhook::unifptr_t{ T::func }.ptr) };
                return *reinterpret_cast<decltype(T::func)*>(&result);
            }
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t&& func) noexcept -> decltype(func_t::raw)
        {
            auto result{ xhook::add_hook(this->value, std::forward<func_t>(func)) };
            return *reinterpret_cast<decltype(func_t::raw)*>(&result);
        }

        template<xhook::is_function_pointer T>
        inline auto operator->*(T func) noexcept -> T 
        {
            auto result{ xhook::add_hook(this->value, func) };
            return *reinterpret_cast<T*>(&result);
        }

    };
    
    struct rva 
    {
        using rva_type = rva;

        void* value;

        inline rva(void* _rva) noexcept : value{ _rva }
        {
        }

        inline rva(uintptr_t _rva) noexcept
            : value{ reinterpret_cast<void*>(_rva) }
        {
        }

        template<xhook::has_func_constexpr T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_pointer_entry<T::func, xhook::rva, T>
        {
            return { this->value };
        }

        template<class T>
        requires (xhook::has_call_target<T> && !xhook::has_func_constexpr<T>)
        inline auto operator&&(T) noexcept -> xhook::pointer_entry<rva, decltype(T::call)>
        {
            return { *this, T::call };
        }
        
        template<xhook::is_xhook_function func_t>
        inline auto operator&&(func_t&& func) noexcept -> xhook::pointer_entry<rva, func_t>
        {
            return { *this, func };
        }

        template<class T>
        requires xhook::is_hook_target<T> || xhook::has_func_constexpr<T>
        inline auto operator->*(T) noexcept -> decltype(T::func)
        {
            auto ptr{ reinterpret_cast<void*>(uintptr_t(xhook::get_base()) + uintptr_t(this->value))};
            if constexpr (xhook::is_hook_target<T> || xhook::has_call_target<T>)
            {
                if constexpr (xhook::has_func_constexpr<T>) 
                {
                    void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::func }.ptr) };
                    *reinterpret_cast<void**>(&T::call) = result;
                }
                else 
                {
                    void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::call }.ptr) };
                    *reinterpret_cast<void**>(&T::call) = result;
                }
                return T::call;
            }
            else 
            {
                void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::func }.ptr) };
                return *reinterpret_cast<decltype(&T::func)>(&result);
            }
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t&& func) noexcept -> decltype(func_t::raw)
        {
            auto ptr{ uintptr_t(xhook::get_base()) + uintptr_t(this->value)};
            auto result{ xhook::add_hook(reinterpret_cast<void*>(ptr), func) };
            return *reinterpret_cast<decltype(func_t::raw)*>(&result);
        }

        template<xhook::is_function_pointer T>
        inline auto operator->*(T func) noexcept -> T
        {
            auto ptr{ uintptr_t(xhook::get_base()) + uintptr_t(this->value)};
            auto result{ xhook::add_hook(reinterpret_cast<void*>(ptr), func) };
            return *reinterpret_cast<T*>(&result);
        }

    };

    template<class T>
    concept is_rva = requires(std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T>, T> p)
    {
        typename decltype(p)::rva_type;
        requires std::same_as<decltype(p), typename decltype(p)::rva_type>;
        { p.value } -> std::convertible_to<void*>;
    };

    template<class E>
    concept is_rva_entry = requires
    {
        typename E::pointer_type;
        requires xhook::is_rva<typename E::pointer_type>;
    };

    template<class T>
    concept is_rva_hook_entry = (xhook::is_pointer_entry<T> && requires(T e) { requires xhook::is_rva<decltype(e.pointer)>; }) ||
                                (xhook::is_fixed_function_pointer_entry<T> && xhook::is_rva<typename T::pointer_type>);

    struct nullptr_t
    {
        template<xhook::has_func_constexpr T>
        inline auto operator&&(T) noexcept -> xhook::empty_entry<T::func>
        {
            return {};
        }
    };

    template<class T>
    struct invoke_function_traits;
    
    #define xhook_declare_invoke_caller(...)/********************************************************/\
    /**/    template<class ret, class... args>                                                        \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__*)(args...)>                          \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(args... value) noexcept                                 \
    /**/            :  values{ std::forward<args>(value)... }                                         \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /**/                                                                                              \
    /**/    template<class ret, class... args>                                                        \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__*)(args...) noexcept>                 \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(args... value) noexcept                                 \
    /**/            :  values{ std::forward<args>(value)... }                                         \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /**/                                                                                              \
    /**/    template<class ret, class clazz, class... args>                                           \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__ clazz::*)(args...)>                  \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        clazz*              object;                                                           \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(clazz* m_this, args... value) noexcept                  \
    /**/            :  object{ std::forward<clazz*>(m_this) },                                        \
    /**/               values{ std::forward<args>(value)... }                                         \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /**/                                                                                              \
    /**/    template<class ret, class clazz, class... args>                                           \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__ clazz::*)(args...) const>            \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        const clazz*        object;                                                           \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(const clazz* m_this, args... value) noexcept            \
    /**/            :  object{ std::forward<const clazz*>(m_this) },                                  \
    /**/               values{ std::forward<args>(value)...       }                                   \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /**/                                                                                              \
    /**/    template<class ret, class clazz, class... args>                                           \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__ clazz::*)(args...) noexcept>         \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        clazz*              object;                                                           \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(clazz* m_this, args... value) noexcept                  \
    /**/            :  object{ std::forward<clazz*>(m_this) },                                        \
    /**/               values{ std::forward<args>(value)... }                                         \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /**/                                                                                              \
    /**/    template<class ret, class clazz, class... args>                                           \
    /**/    struct xhook::invoke_function_traits<ret(__VA_ARGS__ clazz::*)(args...) const noexcept>   \
    /**/    {                                                                                         \
    /**/        using ret_type = ret;                                                                 \
    /**/                                                                                              \
    /**/        const clazz*        object;                                                           \
    /**/        std::tuple<args...> values;                                                           \
    /**/                                                                                              \
    /**/        inline invoke_function_traits(const clazz* m_this, args... value) noexcept            \
    /**/            :  object{ std::forward<const clazz*>(m_this) },                                  \
    /**/               values{ std::forward<args>(value)...       }                                   \
    /**/        {                                                                                     \
    /**/        }                                                                                     \
    /**/    };                                                                                        \
    /*************************************************************************************************/

    #ifdef _M_IX86
    xhook_declare_invoke_caller(__cdecl   )
    xhook_declare_invoke_caller(__stdcall )
    xhook_declare_invoke_caller(__fastcall)
    xhook_declare_invoke_caller(__thiscall)
    #else
    xhook_declare_invoke_caller()
    #endif

    template<auto _func, class T = decltype(_func)>
    requires xhook::is_function_pointer<T>
    struct invoke : xhook::invoke_function_traits<T>
    {
        using  funcptr_t = T;
        inline constexpr static funcptr_t func = _func;
        
        using m_this   = xhook::invoke_function_traits<T>;
        using ret_type = typename m_this::ret_type;
        using m_this::m_this;

        auto operator()(funcptr_t func) noexcept -> ret_type
        {
            if constexpr (std::is_member_function_pointer_v<funcptr_t>) 
            {
                return std::apply(std::bind_front(func, m_this::object), m_this::values);
            }
            else 
            {
                return std::apply(func, m_this::values);
            }
        }
    };

    namespace call
    {
        template<auto func>
        requires xhook::is_function_pointer<decltype(func)>
        using call = xhook::invoke<func>;
    }

    template<class T>
    concept is_invoke = requires(T invoke, typename T::funcptr_t func)
    {
        typename T::ret_type;
        typename T::funcptr_t;

        requires xhook::has_func_constexpr<T>;
        requires std::same_as<typename T::funcptr_t, std::remove_cvref_t<decltype(T::func)>>;
        
        { invoke(func) } -> std::same_as<typename T::ret_type>;
    };

    template<class ...T>
    requires (xhook::has_func_constexpr<T> && ...)
    struct entry_indexer
    {
        constexpr static inline auto no_exists{ static_cast<size_t>(-1) };

        template<auto func>
        consteval static auto get_index_if_exists() noexcept -> size_t
        {
            size_t index{ no_exists }, i{ 0 };
            ([&]
            {
                using func_t = decltype(func);
                using call_t = std::remove_cvref_t<decltype(T::func)>;
                if constexpr (std::is_same_v<func_t, call_t>)
                {
                    if (func == T::func)
                    {
                        index = i;
                    }
                }
                ++i;
            }(), ...);
            return index;
        }

        template<size_t index>
        requires (index >= 0 && index < sizeof...(T))
        using entry_t = std::tuple_element_t<index, std::tuple<T...>>;

        template<size_t index>
        requires (index >= 0 && index < sizeof...(T))
        using func_t = decltype(std::tuple_element_t<index, std::tuple<T...>>::func);

        template<size_t index>
        requires (index >= 0 && index < sizeof...(T))
        inline static constexpr auto func = std::tuple_element_t<index, std::tuple<T...>>::func;
    };

    template<class ...T>
    requires (xhook::has_func_constexpr<T> && ...)
    class hooks
    {
        using indexer = xhook::entry_indexer<T...>;

        template<size_t index>
        using func_t  = typename indexer::template  func_t<index>;

        template<size_t index>
        using entry_t = typename indexer::template entry_t<index>;

        template<size_t index>
        inline constexpr static auto func{ indexer::template func<index> };

        template<auto func>
        consteval static auto get_index_if_exists() noexcept -> size_t 
        {
            return indexer::template get_index_if_exists<func>();
        }

        template <class E>
        struct entry_has_ptr
        {
            static constexpr bool value = requires(E e)
            {
                { e.ptr } -> std::convertible_to<void*>;
            };
        };

        template <class E>
        struct entry_has_pointer
        {
            static constexpr bool value = requires(E e)
            {
                { e.pointer } -> std::convertible_to<void*>;
            };
        };

        void* ptrs[sizeof...(T)]{};

    public:

        inline hooks(T... _hooks) noexcept
        {
            size_t i{};
			([&]
			{
				if constexpr (hooks::entry_has_ptr<T>::value)
				{
                    this->ptrs[i] = _hooks.ptr;
				}
                else if constexpr (hooks::entry_has_pointer<T>::value) 
                {
                    this->ptrs[i] = _hooks.pointer;
                }
				++i;
			}(), ...);
        }

        template<auto func>
		requires (hooks::get_index_if_exists<func>() != indexer::no_exists)
		inline auto set(void* ptr) noexcept -> void
		{
            constexpr auto index{ hooks::get_index_if_exists<func>() };
            if constexpr (xhook::is_rva_entry<hooks::entry_t<index>>)
            {
                auto _ptr = uintptr_t(this->ptrs[index]) + uintptr_t(ptr);
                ptr = reinterpret_cast<void*>(_ptr);
            }
            this->ptrs[index] = ptr;
		}

        template<auto func>
		requires (hooks::get_index_if_exists<func>() != indexer::no_exists)
		inline auto set(void* ptr, bool ignore_rva) noexcept -> void
		{
            if (ignore_rva)
            {
                this->ptrs[hooks::get_index_if_exists<func>()] = ptr;
            }
            else 
            {
                this->set<func>(ptr);
            }
		}

        template<auto func, xhook::is_function_pointer ptr_t>
        requires (hooks::get_index_if_exists<func>() != indexer::no_exists)
        inline auto set(ptr_t ptr) noexcept -> void 
        {
            constexpr auto index{ hooks::get_index_if_exists<func>() };
            this->ptrs[index] = xhook::unifptr_t{ ptr }.ptr;
        }

		template<auto func>
		requires (hooks::get_index_if_exists<func>() != indexer::no_exists)
		consteval inline auto get_index() const -> size_t
		{
			return hooks::get_index_if_exists<func>();
		}

		template<auto func>
		requires (hooks::get_index_if_exists<func>() != indexer::no_exists)
		inline auto get_raw() const noexcept -> const decltype(func)&
		{
			return *reinterpret_cast<const decltype(func)*>
            (
                &this->ptrs[hooks::get_index_if_exists<func>()]
            );
		}

		template<size_t index>
		requires (index >=0 && index < sizeof...(T))
		inline auto get_raw() const noexcept -> const hooks::func_t<index>&
		{
			return *reinterpret_cast<const hooks::func_t<index>*>(&this->ptrs[index]);
		}

		inline auto commit() noexcept -> bool 
		{
            void* calls[]{ (xhook::unifptr_t{ T::func }.ptr)... };
            return xhook::add_hooks(this->ptrs, calls, sizeof...(T));
		}

        inline auto undo() noexcept -> bool 
        {
            void* calls[]{ (xhook::unifptr_t{ T::func }.ptr)... };
            return xhook::add_hooks(this->ptrs, calls, sizeof...(T));
        }

        template<xhook::is_invoke invoke_t>
        requires (hooks::get_index_if_exists<invoke_t::func>() != indexer::no_exists)
        auto operator->*(invoke_t invoke) -> typename invoke_t::ret_type
        {
            return invoke(this->get_raw<invoke_t::func>());
        }

    };

    template<class T>
    concept is_fixed_function_rva_entry = xhook::is_fixed_function_pointer_entry<T> && 
                                          xhook::is_rva<typename T::pointer_type>;

    template<class ...T>
    requires (xhook::is_fixed_function_rva_entry<T> && ...)
    class rva_hooks
    {

        using indexer = xhook::entry_indexer<T...>;

        template<size_t index>
        using func_t  = typename indexer::template  func_t<index>;

        template<size_t index>
        using entry_t = typename indexer::template entry_t<index>;

        template<size_t index>
        inline constexpr static auto func{ indexer::template func<index> };

        template<auto func>
        consteval static auto get_index_if_exists() noexcept -> size_t 
        {
            return indexer::template get_index_if_exists<func>();
        }

        void* rvas[sizeof...(T)]{};
        void* ptrs[sizeof...(T)]{};

    public:

        inline rva_hooks(T... hooks) noexcept : rvas{ hooks.ptr... }
        {
        }

        inline auto commit(void* base) noexcept -> bool 
        {
            if (base != nullptr)
            {
                void* calls[]{ (xhook::unifptr_t{ T::func }.ptr)... };
                for (size_t i{}; i < sizeof...(T); i++)
                {
                    auto ptr{ uintptr_t(this->rvas[i]) + uintptr_t(base) };
                    this->ptrs[i] = reinterpret_cast<void*>(ptr);
                }
                return xhook::add_hooks(this->ptrs, calls, sizeof...(T));
            }
            return false;
        }

        template<class char_t = char>
        requires std::same_as<char_t, char   > || std::same_as<char_t, char8_t > || 
                 std::same_as<char_t, wchar_t> || std::same_as<char_t, char16_t>
        inline auto commit(const char_t* module) noexcept -> bool
        {
            if constexpr (sizeof(char_t) == sizeof(char))
            {
                return this->commit(xhook::get_base(*reinterpret_cast<const char**>(&module)));
            }
            else 
            {
                return this->commit(xhook::get_base(*reinterpret_cast<const wchar_t**>(&module)));
            }
        }
       
        inline auto commit() noexcept -> bool
        {
            return this->commit(xhook::get_base());
        }

        template<auto func>
		requires (rva_hooks::get_index_if_exists<func>() != indexer::no_exists)
		consteval inline auto get_index() const -> size_t
		{
			return rva_hooks::get_index_if_exists<func>();
		}

        template<auto func>
		requires (rva_hooks::get_index_if_exists<func>() != indexer::no_exists)
        inline auto get_rva() const noexcept -> void*
		{
			return this->rvas[rva_hooks::get_index_if_exists<func>()];
		}

        template<size_t index>
		requires (index >=0 && index < sizeof...(T))
        inline auto get_rva() const noexcept -> void*
		{
			return this->rvas[index];
		}

		template<auto func>
		requires (rva_hooks::get_index_if_exists<func>() != indexer::no_exists)
		inline auto get_raw() const noexcept -> const decltype(func)&
		{
			return *reinterpret_cast<const decltype(func)*>
            (
                &this->ptrs[rva_hooks::get_index_if_exists<func>()]
            );
		}

		template<size_t index>
		requires (index >=0 && index < sizeof...(T))
		inline auto get_raw() const noexcept -> const rva_hooks::func_t<index>&
		{
			return *reinterpret_cast<const rva_hooks::func_t<index>*>(&this->ptrs[index]);
		}

        template<xhook::is_invoke invoke_t>
        requires (rva_hooks::get_index_if_exists<invoke_t::func>() != indexer::no_exists)
        auto operator->*(invoke_t invoke) -> typename invoke_t::ret_type
        {
            return invoke(this->get_raw<invoke_t::func>());
        }
    };

    template<auto _func>
    struct fixed_function_symbol_entry
    {
        const char* symbol;
        inline constexpr static decltype(_func) func = _func;

        inline fixed_function_symbol_entry(const char* _symbol) noexcept: symbol{ _symbol }
        {
        }
    };

    template<class T>
    concept is_fixed_function_symbol_entry = requires(T entry)
    {
        { entry.symbol } -> std::convertible_to<const char*>;
        requires xhook::has_func_constexpr<T>;
    };

    template<class ...T>
    requires (xhook::is_fixed_function_symbol_entry<T> && ...)
    class symbol_hooks
    {

        using indexer = xhook::entry_indexer<T...>;

        template<size_t index>
        using func_t  = typename indexer::template  func_t<index>;

        template<size_t index>
        using entry_t = typename indexer::template entry_t<index>;

        template<size_t index>
        inline constexpr static auto func{ indexer::template func<index> };

        template<auto func>
        consteval static auto get_index_if_exists() noexcept -> size_t 
        {
            return indexer::template get_index_if_exists<func>();
        }

        const char* syms[sizeof...(T)]{};
        void* ptrs[sizeof...(T)]{};

    public:

        inline symbol_hooks(T... hooks) noexcept : syms{ hooks.symbol ... }
        {
        }

        inline auto commit(void* base) noexcept -> bool
        {
            if (base != nullptr)
            {
                void* calls[]{ (xhook::unifptr_t{ T::func }.ptr)... };
                for (size_t i{}; i < sizeof...(T); i++)
                {
                    if(this->syms[i] != nullptr)
                    {
                        this->ptrs[i] = xhook::get_addr(base, this->syms[i]);
                    }
                }
                return xhook::add_hooks(this->ptrs, calls, sizeof...(T));
            }
            return false;
        }

        template<class char_t = char>
        requires std::same_as<char_t, char   > || std::same_as<char_t, char8_t > || 
                 std::same_as<char_t, wchar_t> || std::same_as<char_t, char16_t>
        inline auto commit(const char_t* module) noexcept -> bool
        {
            if constexpr (sizeof(char_t) == sizeof(char))
            {
                return this->commit(xhook::get_base(*reinterpret_cast<const char**>(&module)));
            }
            else 
            {
                return this->commit(xhook::get_base(*reinterpret_cast<const wchar_t**>(&module)));
            }
        }

        template<auto func>
		requires (symbol_hooks::get_index_if_exists<func>() != indexer::no_exists)
        inline auto get_sym() const noexcept -> const char*
		{
			return this->syms[rva_hooks::get_index_if_exists<func>()];
		}

        template<size_t index>
		requires (index >=0 && index < sizeof...(T))
        inline auto get_sym() const noexcept -> const char*
		{
			return this->syms[index];
		}

        template<auto func>
		requires (symbol_hooks::get_index_if_exists<func>() != indexer::no_exists)
		inline auto get_raw() const noexcept -> const decltype(func)&
		{
			return *reinterpret_cast<const decltype(func)*>
            (
                &this->ptrs[symbol_hooks::get_index_if_exists<func>()]
            );
		}

		template<size_t index>
		requires (index >=0 && index < sizeof...(T))
		inline auto get_raw() const noexcept -> const symbol_hooks::func_t<index>&
		{
			return *reinterpret_cast<const symbol_hooks::func_t<index>*>(&this->ptrs[index]);
		}

        template<xhook::is_invoke invoke_t>
        requires (symbol_hooks::get_index_if_exists<invoke_t::func>() != indexer::no_exists)
        auto operator->*(invoke_t invoke) -> typename invoke_t::ret_type
        {
            return invoke(this->get_raw<invoke_t::func>());
        }

    };

    template <class sym_t, class func_t>
    concept symbol_entry_types = requires(sym_t symbol)
    {
        { symbol.data } -> std::convertible_to<const char*>;
        requires xhook::is_xhook_function<func_t> || xhook::is_function_pointer<func_t>;
        requires !std::is_same_v<typename xhook::get_funcptr<func_t>::type, void>;
    };
    
    template <class sym_t, class func_t>
    requires xhook::symbol_entry_types<sym_t, func_t>
    struct symbol_entry
    {
      
        sym_t &   symbol;
        func_t& function;
        
        using funcptr_t = typename xhook::get_funcptr<func_t>::type;
        
        inline constexpr symbol_entry(sym_t& _symbol, func_t& _function) noexcept : 
            symbol{ _symbol }, function{ _function }
        {
        }
    };

    template <class sym_t, class func_t>
    using symety_t = xhook::symbol_entry<sym_t, func_t>;

    template <class T>
    concept is_symbol_hook_entry = requires(T entry)
    {
        typename T::funcptr_t;
        { entry.symbol.data } -> std::convertible_to<const char*>;
        { xhook::add_hook((void*)(nullptr), entry.function) } -> std::convertible_to<typename T::funcptr_t>;
    } 
    || requires(T entry) 
    {
        typename T::funcptr_t;
        requires xhook::is_function_pointer<decltype(entry.function)>;
        { entry.function    } -> std::convertible_to<typename T::funcptr_t>;
        { entry.symbol.data } -> std::convertible_to<const char*>;
        xhook::add_hook((void*)(nullptr), *(void**)(&entry.function));
    };

    template <size_t N>
    struct string_symbol
    {
        char data[N]{};
        size_t size{ N > 0 ? N - 1 : 0 };
        
        consteval string_symbol(const char(&str)[N]) noexcept
        {
            std::copy_n(str, N, data);
        }
    };

    template <class char_t, size_t N>
    requires (sizeof(char_t) == sizeof(char) || sizeof(char_t) == sizeof(wchar_t))
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

    template <xhook::string_symbol str>
    struct symbol
    {

        template<xhook::has_func_constexpr T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_symbol_entry<T::func>
        {
            return { str.data };
        }

        template <xhook::is_xhook_function func_t, class symstr_t = const decltype(str)>
        inline auto operator->*(func_t&& func) noexcept -> xhook::symbol_entry<symstr_t, func_t>
        {
            return { str, func };
        }

        template<xhook::has_call_target T, class symstr_t = const decltype(str)>
        inline auto operator->*(T) noexcept -> xhook::symbol_entry<symstr_t, decltype(T::call)>
        {
            return { str, T::call };
        }

        template<xhook::has_call_target T>
        inline auto operator>>(T) noexcept -> decltype(T::call)
        {
            void* ptr{ xhook::get_addr(str.data) };
            if (ptr != nullptr)
            {
                void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::call }.ptr) };
                *reinterpret_cast<void**>(&T::call) = result;
                return T::call;
            }
            return nullptr;
        }

        template<xhook::is_xhook_function T>
        inline auto operator>>(T) noexcept -> decltype(T::raw)
        {
            void* ptr{ xhook::get_addr(str.data)};
            if (ptr != nullptr) 
            {
                void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::get() }.ptr)};
                *reinterpret_cast<void**>(&T::raw) = result;
                return T::raw;
            }
            return nullptr;
        }

    };

    namespace sym
    {
        template<xhook::string_symbol _symbol>
        constexpr auto operator""_sym() noexcept -> xhook::symbol<_symbol>
        {
            return {};
        }
    }

    struct symstr
    {
        const char* data;

        template<class T> requires
        requires(T t) { { t.data() } -> std::convertible_to<const char*>; }
        constexpr inline symstr(T&& str) noexcept : data{ reinterpret_cast<const char*>(str.data()) }
        {
        }

        constexpr inline symstr(const char* _data) noexcept : data{ _data }
        {
        }

        template<xhook::has_func_constexpr T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_symbol_entry<T::func>
        {
            return { this->data };
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t&& func) noexcept -> xhook::symbol_entry<symstr, func_t>
        {
            return { *this, func };
        }

        template<xhook::has_call_target T>
        inline auto operator->*(T) noexcept -> xhook::symbol_entry<symstr, decltype(T::call)>
        {
            return { *this, T::call };
        }

        template<xhook::has_call_target T>
        inline auto operator>>(T) noexcept -> decltype(T::call)
        {
            void* ptr{ xhook::get_addr(this->data) };
            if (ptr != nullptr)
            {
                void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::call }.ptr) };
                *reinterpret_cast<void**>(&T::call) = result;
                return T::call;
            }
            return nullptr;
        }

        template<xhook::is_xhook_function T>
        inline auto operator>>(T) noexcept -> decltype(T::raw)
        {
            void* ptr{ xhook::get_addr(this->data) };
            if (ptr != nullptr) 
            {
                void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ T::get() }.ptr)};
                *reinterpret_cast<void**>(&T::raw) = result;
                return T::raw;
            }
            return nullptr;
        }
    };

    struct xmodule
    {
        void* const base;

        inline xmodule() noexcept : base{ xhook::get_base() }
        {
        };

        inline xmodule(const char*    name) noexcept: base{ xhook::get_base(name) }
        {
        }

        inline xmodule(const wchar_t* name) noexcept: base{ xhook::get_base(name) }
        {
        }

        inline xmodule(void* base) noexcept : base{ base }
        {
        }

        template<auto func>
        inline auto add_hook(xhook::is_rva auto rva) const noexcept -> decltype(func)
        {
            if (rva.value != nullptr)
            {
                auto ptr{ uintptr_t(base) + uintptr_t(rva.value) };
                return xhook::hooker::add<func>(reinterpret_cast<void*>(ptr));
            }
            return nullptr;
        }

        template<auto func>
        inline auto add_hook(const char* symbol) const noexcept -> decltype(func)
        {
            if (this->base != nullptr)
            {
                void* const ptr{ xhook::get_addr(this->base, symbol) };
                if (ptr != nullptr)
                {
                    return xhook::hooker::add<func>(ptr);
                }
            }
            return nullptr;
        }

        template<auto func, class T> requires
        requires(T t) { { t.data() } -> std::convertible_to<const char*>; }
        inline auto add_hook(T&& symbol) const noexcept -> decltype(func)
        {
            return this->add_hook<func>(reinterpret_cast<const char*>(symbol.data()));
        }

        template<xhook::is_symbol_hook_entry entry_t>
        inline auto add_hook(entry_t&& entry) const noexcept -> typename entry_t::funcptr_t
        {
            if (this->base != nullptr)
            {
                void* const ptr{ xhook::get_addr(this->base, entry.symbol.data) };
                if (ptr != nullptr)
                {
                    if constexpr (xhook::is_function_pointer<decltype(entry.function)>)
                    {
                        auto result{ xhook::add_hook(ptr, xhook::unifptr_t{ entry.function }.ptr) };
                        *reinterpret_cast<void**>(&entry.function) = result;
                        return entry.function;
                    }
                    else
                    {
                        auto result{ xhook::add_hook(ptr, entry.function) };
                        return reinterpret_cast<typename entry_t::funcptr_t>(result);
                    }
                }
            }
            return nullptr;
        }

        template<xhook::is_rva_hook_entry entry_t>
        requires (!std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        inline auto add_hook(entry_t&& entry) const noexcept -> typename xhook::get_funcptr<entry_t>::type
        {
            using ret_type = typename xhook::get_funcptr<entry_t>::type;

            if (this->base != nullptr)
            {
                if constexpr (xhook::is_fixed_function_pointer_entry<entry_t>)
                {
                    auto ptr{ reinterpret_cast<void*>(uintptr_t(this->base) + uintptr_t(entry.ptr)) };
                    if constexpr (xhook::has_call_target<typename entry_t::function_type>) 
                    {
                        using function = typename entry_t::function_type;
                        void* result{ xhook::add_hook(ptr, xhook::unifptr_t{ entry_t::func }.ptr) };
                        *reinterpret_cast<void**>(&function::call) = result;
                        return *reinterpret_cast<ret_type*>(&function::call);
                    }
                    else 
                    {
                        auto result{ xhook::add_hook(reinterpret_cast<void*>(ptr), xhook::unifptr_t{ entry_t::func }.ptr) };
                        return *reinterpret_cast<ret_type*>(&result);
                    }
                }
                else 
                {
                    auto ptr{ reinterpret_cast<void*>(uintptr_t(this->base) + uintptr_t(entry.pointer.value)) };
                    if constexpr (xhook::is_function_pointer<decltype(entry.function)>)
                    {
                        auto result{ xhook::add_hook(ptr, xhook::unifptr_t{ entry.function }.ptr) };
                        *reinterpret_cast<void**>(&entry.function) = result;
                        return *reinterpret_cast<ret_type*>(&entry.function);
                    }
                    else
                    {
                        auto result{ xhook::add_hook(ptr, xhook::unifptr_t{ entry.function }.ptr) };
                        return *reinterpret_cast<ret_type*>(&result);
                    }
                }
            }
            return nullptr;
        }

        template<xhook::is_symbol_hook_entry entry_t>
        auto operator&=(entry_t&& entry) const noexcept -> typename entry_t::funcptr_t
        {
            return this->add_hook(std::move(entry));
        }

        template<xhook::is_rva_hook_entry entry_t>
        requires (!std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        auto operator&=(entry_t&& entry) const noexcept -> typename xhook::get_funcptr<entry_t>::type
        {
            return this->add_hook(std::move(entry));
        }

        template<class entry_t>
        requires xhook::is_symbol_hook_entry<entry_t> || (xhook::is_rva_hook_entry<entry_t> && 
                 !std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        auto operator+=(entry_t&& entry) const noexcept -> const xmodule&
        {
            this->add_hook(std::move(entry));
            return *this;
        }

        template<class entry_t>
        requires xhook::is_symbol_hook_entry<entry_t> || (xhook::is_rva_hook_entry<entry_t> && 
                 !std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        auto operator+=(entry_t&& entry) noexcept -> xmodule&
        {
            this->add_hook(std::move(entry));
            return *this;
        }

        template<class entry_t>
        requires xhook::is_symbol_hook_entry<entry_t> || (xhook::is_rva_hook_entry<entry_t> && 
                 !std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        auto operator|(entry_t&& entry) const noexcept -> const xmodule&
        {
            this->add_hook(std::move(entry));
            return *this;
        }

        template<class entry_t>
        requires xhook::is_symbol_hook_entry<entry_t> || (xhook::is_rva_hook_entry<entry_t> && 
                 !std::is_same_v<typename xhook::get_funcptr<entry_t>::type, void>)
        auto operator|(entry_t&& entry) noexcept -> xmodule&
        {
            this->add_hook(std::move(entry));
            return *this;
        }

    };

    namespace dll
    {
        template <xhook::string_dll _dll>
        constexpr auto operator""_dll() noexcept -> xhook::xmodule
        {
            if constexpr (sizeof(decltype(_dll.data[0])) == sizeof(char))
            {
                return reinterpret_cast<const char*>(_dll.data);
            }
            else 
            {
                return reinterpret_cast<const wchar_t*>(_dll.data);
            }
        }
    }
}
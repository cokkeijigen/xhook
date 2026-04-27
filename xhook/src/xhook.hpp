#pragma once
#include <type_traits>
#include <functional>
#include <tuple>

namespace xhook
{
	extern auto get_base(void) noexcept -> void*;
	extern auto get_base(const char* module) noexcept -> void*;
	extern auto get_base(const wchar_t* module) noexcept -> void*;

	extern auto get_addr(const char* symbol) noexcept -> void*;
	extern auto get_addr(void* const base, const char* symbol) noexcept -> void*;

	extern auto   init(void) noexcept -> void;
	extern auto commit(void) noexcept -> bool;

	extern auto add_hook (void**  raw, void*   target,  bool commit = true) noexcept -> bool;
	extern auto add_hooks(void** raws, void** targets, size_t count, bool commit = true) noexcept -> bool;

	extern auto unhook (void** raw,  void*  target,  bool commit = true) noexcept -> bool;
	extern auto unhooks(void** raws, void** targets, size_t count, bool commit = true) noexcept -> bool;

    inline auto get_addr(const char* module, const char* symbol) noexcept -> void*
    {
        return xhook::get_addr(xhook::get_base(module), symbol);
    }

    inline auto get_addr(const wchar_t* module, const char* symbol) noexcept -> void*
    {
        return xhook::get_addr(xhook::get_base(module), symbol);
    }

	inline auto add_hook(void* raw, void* target) noexcept -> void* 
	{
		return xhook::add_hook(&raw, target, true) ? raw : nullptr;
	}

	inline auto unhook(void* raw, void* target) noexcept -> bool
	{
		return xhook::unhook(&raw, target, true);
	}

    template<class T>
    struct pointer_rank     : std::integral_constant<size_t, 0> {};

    template<class T>
    struct pointer_rank<T*> : std::integral_constant<size_t, xhook::pointer_rank<T>::value + 1> {};

    template<class T>
    inline constexpr size_t pointer_rank_v = xhook::pointer_rank<T>::value;

	template<class T>
    concept is_function_pointer = std::is_member_function_pointer_v<std::remove_reference_t<T>> ||
                                  std::is_function_v<std::remove_pointer_t<std::remove_reference_t<T>>>;
    
	template<class T = void*>
    requires (xhook::is_function_pointer<T> || std::is_pointer<T>::value)
    union union_pointer
    {
        T raw;
        void* ptr;

        inline union_pointer(T _raw) noexcept requires (!std::is_same_v<T, void*>) : raw{ _raw }
        {
        }

        inline union_pointer(void* _ptr) noexcept : ptr{ _ptr }
        {
        }

        inline union_pointer(std::integral auto _ptr) noexcept 
        {
            this->ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(_ptr));
        }

    };

    template<class T = void*>
    using uniptr_t = xhook::union_pointer<T>;

    template<class caller>
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
    struct is_backup_context<xhook::backup_context<caller>> : std::true_type {};

    template<class T>
    concept backup = is_backup_context<T>::value;

    template<class T, class = void>
    struct is_simple_lambda : std::false_type {};

    template<class T>
    struct is_simple_lambda<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

    template<class T, class caller, class = void>
    struct is_template_backup_lambda : std::false_type {};

    template<class T, class caller>
    struct is_template_backup_lambda<T, caller, 
        std::void_t<decltype(&T::template operator()<backup_context<caller>{}>)>
    > : std::true_type {};

    template<class T>
    concept is_xhook_function = requires
    {
        *reinterpret_cast<void**>(&std::remove_reference_t<T>::raw) = nullptr;
        { std::remove_reference_t<T>::raw   } -> std::convertible_to<void*>;
        { std::remove_reference_t<T>::get() } -> std::convertible_to<void*>;
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

        using lambda_t  = lambda;
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
	/**/		inline name(lambda _lambda) noexcept : function{ _lambda }                                                          \
	/**/		{                                                                                                                   \
	/**/		}                                                                                                                   \
	/**/		using function::function;                   																	    \
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

    class unhooker
    {

        mutable void* m_raw;
        mutable void* m_tar;

    public:

        unhooker() = default;

        inline auto operator=(unhooker&& other) noexcept -> unhooker&
        {
            this->m_raw = other.m_raw;
            this->m_tar = other.m_tar;
            return *this;
        }

        inline unhooker(unhooker&& other) noexcept : m_raw{ other.m_raw }, m_tar{ other.m_tar }
        {
        }

        inline unhooker(void* const raw, void* const tar) noexcept : m_raw{ raw }, m_tar{ tar }
        {
        }

        inline auto raw() const noexcept -> void*
        {
            return this->m_raw;
        }

        inline auto tar() const noexcept -> void*
        {
            return this->m_tar;
        }

        inline auto unhook(bool commit = true) const noexcept -> bool
        {
            return xhook::unhook(&this->m_raw, this->m_tar, commit);
        };

    };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t = raw_t>
    requires (xhook::pointer_rank_v<raw_t> <= 2 && xhook::pointer_rank_v<tar_t> <= 2)
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
        };
        
        class unhooker 
        {
            mutable void* m_raw;
            mutable void* m_tar;

        public:

            inline auto raw() const noexcept -> void*
            {
                return 
                {
                    (std::is_reference_v<raw_t> || xhook::pointer_rank_v<raw_t> == 2) ?
                    *reinterpret_cast<void**>(this->m_raw) : this->m_raw
                };
            }

            inline auto tar() const noexcept -> void*
            {
                return 
                {
                    (std::is_reference_v<tar_t> || xhook::pointer_rank_v<tar_t> == 2) ?
                    *reinterpret_cast<void**>(this->m_tar) : this->m_tar
                };
            }

            inline auto unhook(bool commit = true) const noexcept -> bool 
            {
                void** raw
                {
                    (std::is_reference_v<raw_t> || xhook::pointer_rank_v<raw_t> == 2) ?
                    reinterpret_cast<void**>(this->m_raw ) :
                    reinterpret_cast<void**>(&this->m_raw)
                };
                
                void* tar
                {
                    (std::is_reference_v<tar_t> || xhook::pointer_rank_v<tar_t> == 2) ?
                    *reinterpret_cast<void**>(this->m_tar) : this->m_tar
                };

                return xhook::unhook(raw, tar, commit);
            }

            inline auto operator*() const noexcept -> xhook::unhooker
            {
                return { this->raw(), this->tar() };
            }
        };

        union uniondata
        {
            result::entry       entry;
            result::unhooker unhooker;
        };

        const uniondata data;

        inline result(raw_t _raw, tar_t _tar) noexcept : data{ .entry{ _raw, _tar } }
        {
        }

        inline auto operator->() const noexcept -> const result::entry*
        {
            return &this->data.entry;
        }

        inline auto operator*() const noexcept -> const result::unhooker&
        {
            return this->data.unhooker;
        }

        template<std::size_t i>
        requires (i >= 0 && i <= 2)
        inline auto&& get() const noexcept
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
    struct std::tuple_element<0, xhook::result<raw_t, tar_t>> { using type = raw_t; };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_element<1, xhook::result<raw_t, tar_t>> { using type = tar_t; };

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct std::tuple_element<2, xhook::result<raw_t, tar_t>> { using type = xhook::result<raw_t, tar_t>::unhooker; };

    template<class raw_t, class tar_t>
    struct result_pointer
    {
        using type = void*;
    };
    
    template<std::integral raw_t, std::integral tar_t>
    struct result_pointer<raw_t, tar_t>
    {
        using type = std::conditional_t<std::is_signed_v<raw_t>, std::intptr_t, std::uintptr_t>;
    };

    template<xhook::is_function_pointer raw_t, class tar_t>
    requires (!xhook::is_function_pointer<tar_t>)
    struct result_pointer<raw_t, tar_t>
    {
        using type = raw_t;
    };

    template<class raw_t, xhook::is_function_pointer tar_t>
    requires (!xhook::is_function_pointer<raw_t>)
    struct result_pointer<raw_t, tar_t>
    {
        using type = tar_t;
    };  

    template<xhook::is_function_pointer raw_t, xhook::is_function_pointer tar_t>
    struct result_pointer<raw_t, tar_t>
    {
        using type = tar_t;
    };

    template<class raw_t, class tar_t>
    using retptr_t = typename xhook::result_pointer<raw_t, tar_t>::type;

    template<class T>
    concept is_valid_pointer = xhook::is_function_pointer<T> || std::is_same_v<T, void*> || std::integral<T>;

    template<xhook::is_valid_pointer raw_t, xhook::is_valid_pointer tar_t>
    static inline auto add_hook(raw_t raw, tar_t tar) noexcept -> retptr_t<raw_t, tar_t> 
    {
        void* _raw{ xhook::uniptr_t{ raw }.ptr  };
        void* _tar{ xhook::uniptr_t{ tar }.ptr  };
        void* _ptr{ xhook::add_hook(_raw, _tar) };
        return *reinterpret_cast<xhook::retptr_t<raw_t, tar_t>*>(&_ptr);
    }

    template<xhook::is_valid_pointer raw_t, xhook::is_valid_pointer tar_t>
    requires (std::integral<raw_t> ? (sizeof(raw_t) >= sizeof(void*)) : true)
    static inline auto add_hook(raw_t* raw, tar_t tar, bool commit = true) noexcept -> bool
    {
        void** _raw{ reinterpret_cast<void**>(xhook::uniptr_t{ raw }.ptr) };
        void*  _tar{ xhook::uniptr_t{ tar }.ptr  };
        return xhook::add_hook(_raw, _tar, commit);
    }

    template<xhook::is_xhook_function func, class ptr_t = decltype(func::raw)>
    static inline auto add_hook(xhook::is_valid_pointer auto raw, func) noexcept -> xhook::result<ptr_t&, ptr_t>
    {
        *reinterpret_cast<void**>(&func::raw) = xhook::uniptr_t{ raw }.ptr;
        void** _raw{ reinterpret_cast<void**>(&func::raw) };
        void*  _tar{ xhook::uniptr_t{ func::get() }.ptr  };
        xhook::add_hook(_raw, _tar, true);
        return { func::raw, func::get() };
    }

    template<xhook::is_function_pointer auto _func>
    struct target
    {
        inline           static decltype(_func) call = _func;
        inline constexpr static decltype(_func) func = _func;

        static inline auto unhook(bool commit = true) noexcept -> bool
        {
            void** raw{ reinterpret_cast<void**>(&xhook::target<_func>::call) };
            void*  tar{ xhook::uniptr_t{ _func }.ptr };
            return xhook::unhook(raw, tar, commit);
        }
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
    concept has_static_call = requires
    {
        *reinterpret_cast<void**>(&T::call) = nullptr;
        requires xhook::is_function_pointer<decltype(T::call)>;
    };

    template<class T>
    concept has_constexpr_func = requires
    {
        requires xhook::is_function_pointer<decltype(T::func)>;
        typename std::integral_constant<decltype(T::func), T::func>;
    };

    template<class T>
    struct function_signature
    {
        using type = void;
    };

    template<xhook::is_function_pointer T>
    struct function_signature<T>
    {
        using type = std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T>, T>;
    };

    template<xhook::is_xhook_function T>
    struct function_signature<T>
    {
        using type = decltype(T::raw);
    };

    template<class T>
    requires xhook::has_constexpr_func<T> || xhook::has_static_call<T>
    struct function_signature<T>
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
    using fnsig  = xhook::function_signature<T>;

    template<class T>
    using fptr_t = typename xhook::fnsig<T>::type;

    struct hooker 
    {
        
        template<xhook::is_function_pointer auto func>
        inline constexpr static decltype(func)& call = xhook::target<func>::call;

        inline static auto commit() noexcept -> bool
        {
            return xhook::commit();
        }

        template<xhook::is_function_pointer auto func>
        inline static auto add(void* raw, bool commit = true) noexcept -> bool
        {
            *reinterpret_cast<void**>(&xhook::target<func>::call) = raw;
            void** _raw{ reinterpret_cast<void**>(&xhook::target<func>::call) };
            void*  _tar{ xhook::uniptr_t{ func }.ptr };
            return xhook::add_hook(_raw, _tar, commit);
        }

        template<xhook::is_function_pointer auto func>
        inline static auto add(std::uintptr_t raw, bool commit = true) noexcept -> bool
        {
            return xhook::hooker::add<func>(reinterpret_cast<void*>(raw), commit);
        }

        template<xhook::is_function_pointer auto func>
        inline static auto add(xhook::is_function_pointer auto raw, bool commit = true) noexcept -> bool
        {
            return xhook::hooker::add<func>(xhook::uniptr_t{ raw }.ptr, commit);
        }

        template<xhook::is_function_pointer auto func>
        inline static auto del(bool commit = true) noexcept -> bool
        {
            void** _raw{ reinterpret_cast<void**>(&xhook::target<func>::call) };
            void*  _tar{ xhook::uniptr_t{ func }.ptr };
            return xhook::unhook(_raw, _tar, commit);
        }
    };

    template<xhook::is_function_pointer auto _func, class ptr_t = void*, xhook::has_static_call tar_t = xhook::target<_func>>
    struct fixed_function_pointer_entry
    {
        void* ptr{};
       
        using pointer_type = ptr_t;
        using target_type  = tar_t;
        
        inline constexpr static decltype(_func) func = _func;

        inline fixed_function_pointer_entry() noexcept {}
        inline fixed_function_pointer_entry(void* _ptr) noexcept : ptr{ _ptr }
        {
        }

    };

    template<xhook::is_function_pointer auto func, class ptr_t = void*, xhook::has_static_call tar_t = xhook::target<func>>
    using fxptrety_t = xhook::fixed_function_pointer_entry<func, ptr_t, tar_t>;

    template<class T>
    concept is_fixed_function_pointer_entry = requires(T entry)
    {
        typename T::pointer_type;
        typename T::target_type;
        requires xhook::has_constexpr_func<T>;
        { entry.ptr } -> std::convertible_to<void*>;
    };

    template<class T>
    concept is_fxptrety = xhook::is_fixed_function_pointer_entry<T>;

    template<class ptr_t, class func_t>
    concept pointer_entry_types = requires(ptr_t ptr)
    {
        { ptr.value } -> std::convertible_to<void*>;
        requires xhook::is_xhook_function<func_t> || xhook::is_function_pointer<func_t>;
        requires !std::is_same_v<typename xhook::fnsig<func_t>::type, void>;
    };

    template<class ptr_t, class func_t>
    requires xhook::pointer_entry_types<ptr_t, func_t>
    struct pointer_entry
    {
        const ptr_t& pointer;
        func_t&     function;

        using type = typename xhook::fnsig<func_t>::type;

        inline pointer_entry(const ptr_t& ptr, func_t& func) noexcept :
               pointer{ ptr }, function{ func }
        {
        }
    };

    template<class ptr_t, class func_t>
    using ptrety_t = xhook::pointer_entry<ptr_t, func_t>;

    template<class T>
    concept is_pointer_entry = requires(T entry)
    {
        typename T::type;
        requires !std::is_same_v<void, typename T::type>;
        requires std::is_reference_v<decltype(entry.function)>;
        { entry.pointer.value  } -> std::convertible_to<void*>;
        xhook::add_hook((void*)(nullptr), entry.function);
    };

    template<xhook::is_pointer_entry T>
    struct function_signature<T> 
    {
        using type = typename T::type;
    };

    struct ptr
    {
        void* value;

        inline ptr(void* ptr) noexcept : value{ ptr }
        {
        }

        inline ptr(std::uintptr_t ptr) noexcept: value{ reinterpret_cast<void*>(ptr) }
        {
        }

        template<xhook::is_function_pointer T>
        inline ptr(T _ptr) noexcept : value{ xhook::uniptr_t{ _ptr }.ptr }
        {
        }

        template<xhook::is_hook_target T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_pointer_entry<T::func, xhook::ptr, T>
        {
            return { this->value };
        }

        template<xhook::is_hook_target T>
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::func)> 
        {
            *reinterpret_cast<void**>(&T::call) = this->value;
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::func }.ptr };
            xhook::add_hook(raw, tar, true);
            return { T::call, T::func };
        }
        
        template<class T>
        requires (xhook::has_static_call<T> && !xhook::has_constexpr_func<T>)
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::call)>
        {
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::call }.ptr    };
            *reinterpret_cast<void**>(&T::call) = this->value;
            xhook::add_hook(raw, tar, true);
            return { T::call, *reinterpret_cast<decltype(&T::call)>(&tar) };
        }

        template<class T>
        requires (!xhook::has_static_call<T> && xhook::has_constexpr_func<T>)
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::func), decltype(T::func)>
        {
            void* raw{ this->value };
            void* tar{ xhook::uniptr_t{ T::call }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            { 
                *reinterpret_cast<decltype(&T::func)>(&raw), 
                *reinterpret_cast<decltype(&T::func)>(&tar)
            };
        }
        
        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t func) noexcept -> xhook::result<decltype(func_t::raw)&, decltype(func_t::get())>
        {
            *reinterpret_cast<void**>(&func_t::raw) = this->value;
            void** raw{ reinterpret_cast<void**>(&func_t::raw) };
            void*  tar{ reinterpret_cast<void*>(func_t::get()) };
            xhook::add_hook(raw, tar, true);
            return { func_t::raw, func_t::get() };
        }

        template<xhook::is_function_pointer T>
        inline auto operator->*(T func) noexcept -> xhook::result<T, T>
        {
            void* raw{ this->value };
            void* tar{ xhook::uniptr_t{ func }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            {
                *reinterpret_cast<T*>(&raw),
                *reinterpret_cast<T*>(&tar)
            };
        }

    };

    struct rva
    {
        using rva_type = rva;
        
        void* value;

        inline rva(void* _rva) noexcept : value{ _rva }
        {
        }

        inline rva(std::uintptr_t _rva) noexcept : value{ reinterpret_cast<void*>(_rva) }
        {
        }

        template<xhook::is_hook_target T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_pointer_entry<T::func, xhook::rva, T>
        {
            return { this->value };
        }

        template<class T>
        requires (xhook::has_static_call<T> && !xhook::has_constexpr_func<T>)
        inline auto operator&&(T) const noexcept -> xhook::pointer_entry<xhook::rva, decltype(T::call)>
        {
            return { *this, T::call };
        }

        template<xhook::is_xhook_function func_t>
        inline auto operator&&(func_t func) const noexcept -> xhook::pointer_entry<xhook::rva, func_t>
        {
            return { *this, func };
        }

        template<xhook::is_hook_target T>
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::func)>
        {
            *reinterpret_cast<void**>(&T::call) = xhook::uniptr_t
            {
                reinterpret_cast<std::uintptr_t>(xhook::get_base()) + 
                reinterpret_cast<std::uintptr_t>(this->value)
            }.ptr;

            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::func }.ptr };
            xhook::add_hook(raw, tar, true);
            return { T::call, T::func };
        }

        template<class T>
        requires (xhook::has_static_call<T> && !xhook::has_constexpr_func<T>)
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::call)>
        {
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::call }.ptr    };
            *reinterpret_cast<void**>(&T::call) = xhook::uniptr_t
            {
                reinterpret_cast<std::uintptr_t>(xhook::get_base()) + 
                reinterpret_cast<std::uintptr_t>(this->value)
            }.ptr;
            xhook::add_hook(raw, tar, true);
            return { T::call, *reinterpret_cast<decltype(&T::call)>(&tar) };
        }

        template<class T>
        requires (!xhook::has_static_call<T> && xhook::has_constexpr_func<T>)
        inline auto operator->*(T) const noexcept -> xhook::result<decltype(T::func), decltype(T::func)>
        {
            auto ptr
            { 
                reinterpret_cast<std::uintptr_t>(xhook::get_base()) +
                reinterpret_cast<std::uintptr_t>(this->value) 
            };
            void** raw{ reinterpret_cast<void**>(&ptr)  };
            void*  tar{ xhook::uniptr_t{ T::call }.ptr };

            xhook::add_hook(raw, tar, true);
            return xhook::result
            { 
                *reinterpret_cast<decltype(&T::func)>( raw),
                *reinterpret_cast<decltype(&T::func)>(&tar)
            };
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t func) noexcept -> xhook::result<decltype(func_t::raw)&, decltype(func_t::get())>
        {
            *reinterpret_cast<std::uintptr_t**>(&func_t::raw) = std::uintptr_t
            {
                reinterpret_cast<std::uintptr_t>(xhook::get_base()) +
                reinterpret_cast<std::uintptr_t>(this->value)
            };
            void** raw{ reinterpret_cast<void**>(&func_t::raw) };
            void*  tar{ reinterpret_cast<void*>(func_t::get()) };
            xhook::add_hook(raw, tar, true);
            return { func_t::raw, func_t::get() };
        }

        template<xhook::is_function_pointer T>
        inline auto operator->*(T func) noexcept -> xhook::result<T, T>
        {
            auto ptr
            {
                reinterpret_cast<std::uintptr_t>(xhook::get_base()) +
                reinterpret_cast<std::uintptr_t>(this->value)
            };
            void** raw{ reinterpret_cast<void**>(&ptr) };
            void*  tar{ xhook::uniptr_t{ func }.ptr   };
            xhook::add_hook(raw, tar, true);
            return xhook::result
            {
                *reinterpret_cast<T**>( raw),
                *reinterpret_cast<T*> (&tar)
            };
        }
    };

    template<class T>
    concept is_rva = requires(std::remove_cvref_t<T> p)
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

    template<auto _func>
    struct empty_entry
    {
        inline constexpr static decltype(_func) func = _func;
    };

    template<auto _func>
    using empety_t = xhook::empty_entry<_func>;

    struct nullptr_t
    {
        template<xhook::has_constexpr_func T>
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
    /**[xhook_declare_invoke_caller]******************************************************************/

    #ifdef _M_IX86
    xhook_declare_invoke_caller(__cdecl   )
    xhook_declare_invoke_caller(__stdcall )
    xhook_declare_invoke_caller(__fastcall)
    xhook_declare_invoke_caller(__thiscall)
    #else
    xhook_declare_invoke_caller()
    #endif

    template<xhook::is_function_pointer auto _func>
    struct invoke : xhook::invoke_function_traits<decltype(_func)>
    {
        using  funcptr_t = decltype(_func);
        inline constexpr static funcptr_t func = _func;
        
        using m_this   = xhook::invoke_function_traits<funcptr_t>;
        using ret_type = typename m_this::ret_type;
        using m_this::m_this;

        auto operator()(funcptr_t ptr) const noexcept -> ret_type
        {
            if constexpr (std::is_member_function_pointer_v<funcptr_t>) 
            {
                return std::apply(std::bind_front(ptr, m_this::object), m_this::values);
            }
            else 
            {
                return std::apply(ptr, m_this::values);
            }
        }
    };

    template<class T>
    concept is_invoke = requires(T invoke, typename T::funcptr_t func)
    {
        typename T::ret_type;
        typename T::funcptr_t;

        requires xhook::has_constexpr_func<T>;
        requires std::same_as<typename T::funcptr_t, std::remove_cvref_t<decltype(T::func)>>;
        
        { invoke(func) } -> std::same_as<typename T::ret_type>;
    };

    namespace call
    {
        template<xhook::is_function_pointer auto func>
        using call = xhook::invoke<func>;
    }

    template<class ...T>
    requires (xhook::has_constexpr_func<T> && ...)
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
    requires (xhook::has_constexpr_func<T> && ...)
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

        mutable void* ptrs[sizeof...(T)]{};

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
                auto _ptr = std::uintptr_t(this->ptrs[index]) + std::uintptr_t(ptr);
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
            this->ptrs[index] = xhook::uniptr_t{ ptr }.ptr;
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

        template<xhook::is_invoke invoke_t>
        requires (hooks::get_index_if_exists<invoke_t::func>() != indexer::no_exists)
        auto operator->*(invoke_t invoke) const -> typename invoke_t::ret_type
        {
            return invoke(this->get_raw<invoke_t::func>());
        }

		inline auto commits() const noexcept -> bool
		{
            void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
            return xhook::add_hooks(this->ptrs, calls, sizeof...(T), true);
		}

        inline auto unhooks() const noexcept -> bool
        {
            void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
            return xhook::unhooks(this->ptrs, calls, sizeof...(T), true);
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
        using func_t = typename indexer::template  func_t<index>;

        template<size_t index>
        using entry_t = typename indexer::template entry_t<index>;

        template<size_t index>
        inline constexpr static auto func{ indexer::template func<index> };

        template<auto func>
        consteval static auto get_index_if_exists() noexcept -> size_t
        {
            return indexer::template get_index_if_exists<func>();
        }

        mutable void* rvas[sizeof...(T)]{};
        mutable void* ptrs[sizeof...(T)]{};

    public:

        inline rva_hooks(T... hooks) noexcept : rvas{ hooks.ptr... }
        {
        }

        inline auto commits(void* base) const noexcept -> bool
        {
            if (base == nullptr)
            {
                return false;
            }
            void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
            for (size_t i{}; i < sizeof...(T); i++)
            {
                auto ptr{ std::uintptr_t(this->rvas[i]) + std::uintptr_t(base) };
                this->ptrs[i] = reinterpret_cast<void*>(ptr);
            }
            return xhook::add_hooks(this->ptrs, calls, sizeof...(T), true);
        }

        template<class char_t = char>
        requires std::same_as<char_t, char   > || std::same_as<char_t, char8_t > || 
                 std::same_as<char_t, wchar_t> || std::same_as<char_t, char16_t>
        inline auto commits(const char_t* module) const noexcept -> bool
        {
            if constexpr (sizeof(char_t) == sizeof(char))
            {
                return this->commits(xhook::get_base(*reinterpret_cast<const char**>(&module)));
            }
            else 
            {
                return this->commits(xhook::get_base(*reinterpret_cast<const wchar_t**>(&module)));
            }
        }

        inline auto commits() const noexcept -> bool
        {
            return this->commit(xhook::get_base());
        }

        inline auto unhooks() const noexcept -> bool
        {
            void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
            return xhook::unhooks(this->ptrs, calls, sizeof...(T), true);
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
        requires xhook::has_constexpr_func<T>;
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

        const char*   syms[sizeof...(T)]{};
        mutable void* ptrs[sizeof...(T)]{};

    public:

        inline symbol_hooks(T... hooks) noexcept : syms{ hooks.symbol ... }
        {
        }

        inline auto unhooks() const noexcept -> bool
        {
            void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
            return xhook::unhooks(this->ptrs, calls, sizeof...(T), true);
        }

        inline auto commits(void* base) const noexcept -> bool
        {
            if (base != nullptr)
            {
                void* calls[]{ (xhook::uniptr_t{ T::func }.ptr)... };
                for (size_t i{}; i < sizeof...(T); i++)
                {
                    if(this->syms[i] != nullptr)
                    {
                        this->ptrs[i] = xhook::get_addr(base, this->syms[i]);
                    }
                }
                return xhook::add_hooks(this->ptrs, calls, sizeof...(T), true);
            }
            return false;
        }

        template<class char_t = char>
        requires std::same_as<char_t, char   > || std::same_as<char_t, char8_t > || 
                 std::same_as<char_t, wchar_t> || std::same_as<char_t, char16_t>
        inline auto commits(const char_t* module) const noexcept -> bool
        {
            if constexpr (sizeof(char_t) == sizeof(char))
            {
                return this->commits(xhook::get_base(*reinterpret_cast<const char**>(&module)));
            }
            else 
            {
                return this->commits(xhook::get_base(*reinterpret_cast<const wchar_t**>(&module)));
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

    template<class sym_t, class func_t>
    concept symbol_entry_types = requires(sym_t symbol)
    {
        { symbol.data } -> std::convertible_to<const char*>;
        requires xhook::is_xhook_function<func_t> || xhook::is_function_pointer<func_t>;
        requires !std::is_same_v<typename xhook::fnsig<func_t>::type, void>;
    };

    template <class sym_t, class func_t>
    requires xhook::symbol_entry_types<sym_t, func_t>
    struct symbol_entry
    {
        sym_t &   symbol;
        func_t& function;

        using type = typename xhook::fnsig<func_t>::type;

        inline symbol_entry(sym_t& _symbol, func_t& _function) noexcept : 
               symbol{ _symbol }, function{ _function }
        {
        }
    };

    template<class sym_t, class func_t>
    using symety_t = xhook::symbol_entry<sym_t, func_t>;

    template <class T>
    concept is_symbol_entry = requires(T entry)
    {
        requires std::is_reference_v<decltype(entry.function)>;
        requires xhook::is_xhook_function  <decltype(entry.function)> ||
                 xhook::is_function_pointer<decltype(entry.function)>;

        requires 
        (
            !xhook::is_xhook_function<std::remove_cvref_t<decltype(entry.function)>> ||
            requires 
            {
                { entry.function.raw   } -> std::convertible_to<typename T::type&>;
                { entry.function.get() } -> std::convertible_to<typename T::type>;
            }
        );

        requires 
        (
            !xhook::is_function_pointer<std::remove_cvref_t<decltype(entry.function)>> || 
            requires 
            {
                { entry.function } -> std::convertible_to<typename T::type&>;
            }
        );

        { entry.symbol.data } -> std::convertible_to<const char*>;
    };

    template <size_t N>
    requires (N > 1)
    struct string_symbol
    {
        char data[N]{};
        size_t size{ N - 1 };
        
        consteval string_symbol(const char(&str)[N]) noexcept
        {
            std::copy_n(str, N, data);
        }
    };

    template<xhook::string_symbol str>
    struct symbol 
    {
        template<xhook::has_constexpr_func T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_symbol_entry<T::func>
        {
            return { str.data };
        }

        template <xhook::is_xhook_function func_t, class symstr_t = const decltype(str)>
        inline auto operator->*(func_t func) noexcept -> xhook::symbol_entry<symstr_t, func_t>
        {
            return { str, func };
        }

        template<xhook::has_static_call T, class symstr_t = const decltype(str)>
        inline auto operator->*(T) noexcept -> xhook::symbol_entry<symstr_t, decltype(T::call)>
        {
            return { str, T::call };
        }

        template<xhook::is_hook_target T>
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::func)>
        {
            *reinterpret_cast<void**>(&T::call) = xhook::get_addr(str.data);
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::func }.ptr };
            xhook::add_hook(raw, tar, true);
            return { T::call, T::func };
        }
        
        template<class T>
        requires (xhook::has_static_call<T> && !xhook::has_constexpr_func<T>)
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::call)>
        {
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::call }.ptr    };
            *reinterpret_cast<void**>(&T::call) = xhook::get_addr(str.data);
            xhook::add_hook(raw, tar, true);
            return { T::call, *reinterpret_cast<decltype(&T::call)>(&tar) };
        }

        template<class T>
        requires (!xhook::has_static_call<T> && xhook::has_constexpr_func<T>)
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::func), decltype(T::func)>
        {
            void* raw{ xhook::get_addr(str.data) };
            void* tar{ xhook::uniptr_t{ T::call }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            { 
                *reinterpret_cast<decltype(&T::func)>(&raw), 
                *reinterpret_cast<decltype(&T::func)>(&tar)
            };
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator>>(func_t func) noexcept -> xhook::result<decltype(func_t::raw)&, decltype(func_t::get())>
        {
            *reinterpret_cast<void**>(&func_t::raw) = xhook::get_addr(str.data);
            void** raw{ reinterpret_cast<void**>(&func_t::raw) };
            void*  tar{ reinterpret_cast<void*>(func_t::get()) };
            xhook::add_hook(raw, tar, true);
            return { func_t::raw, func_t::get() };
        }

        template<xhook::is_function_pointer T>
        inline auto operator>>(T func) noexcept -> xhook::result<T, T>
        {
            void* raw{ xhook::get_addr(str.data)    };
            void* tar{ xhook::uniptr_t{ func }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            {
                *reinterpret_cast<T*>(&raw),
                *reinterpret_cast<T*>(&tar)
            };
        }
    };

    namespace sym
    {
        template<xhook::string_symbol symbol>
        constexpr auto operator""_sym() noexcept -> xhook::symbol<symbol>
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

        template<xhook::has_constexpr_func T>
        inline auto operator&&(T) noexcept -> xhook::fixed_function_symbol_entry<T::func>
        {
            return { this->data };
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator->*(func_t&& func) noexcept -> xhook::symbol_entry<symstr, func_t>
        {
            return { *this, func };
        }

        template<xhook::has_static_call T>
        inline auto operator->*(T) noexcept -> xhook::symbol_entry<symstr, decltype(T::call)>
        {
            return { *this, T::call };
        }

        template<xhook::is_hook_target T>
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::func)>
        {
            *reinterpret_cast<void**>(&T::call) = xhook::get_addr(this->data);
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::func }.ptr };
            xhook::add_hook(raw, tar, true);
            return { T::call, T::func };
        }
        
        template<class T>
        requires (xhook::has_static_call<T> && !xhook::has_constexpr_func<T>)
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::call)&, decltype(T::call)>
        {
            void** raw{ reinterpret_cast<void**>(&T::call) };
            void*  tar{ xhook::uniptr_t{ T::call }.ptr    };
            *reinterpret_cast<void**>(&T::call) = xhook::get_addr(this->data);
            xhook::add_hook(raw, tar, true);
            return { T::call, *reinterpret_cast<decltype(&T::call)>(&tar) };
        }

        template<class T>
        requires (!xhook::has_static_call<T> && xhook::has_constexpr_func<T>)
        inline auto operator>>(T) const noexcept -> xhook::result<decltype(T::func), decltype(T::func)>
        {
            void* raw{ xhook::get_addr(this->data)     };
            void* tar{ xhook::uniptr_t{ T::call }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            { 
                *reinterpret_cast<decltype(&T::func)>(&raw), 
                *reinterpret_cast<decltype(&T::func)>(&tar)
            };
        }

        template <xhook::is_xhook_function func_t>
        inline auto operator>>(func_t func) noexcept -> xhook::result<decltype(func_t::raw)&, decltype(func_t::get())>
        {
            *reinterpret_cast<void**>(&func_t::raw) = xhook::get_addr(this->data);
            void** raw{ reinterpret_cast<void**>(&func_t::raw) };
            void*  tar{ reinterpret_cast<void*>(func_t::get()) };
            xhook::add_hook(raw, tar, true);
            return { func_t::raw, func_t::get() };
        }

        template<xhook::is_function_pointer T>
        inline auto operator>>(T func) noexcept -> xhook::result<T, T>
        {
            void* raw{ xhook::get_addr(this->data)  };
            void* tar{ xhook::uniptr_t{ func }.ptr };
            xhook::add_hook(&raw, tar, true);
            return xhook::result
            {
                *reinterpret_cast<T*>(&raw),
                *reinterpret_cast<T*>(&tar)
            };
        }
    };

    class xmodule
    {
         mutable void* m_base;

    public:

        inline xmodule() noexcept : m_base{ xhook::get_base() }
        {
        };

        inline xmodule(void* base) noexcept : m_base{ base }
        {
        }

        inline xmodule(xhook::xmodule&& other) noexcept : m_base{ other.m_base }
        {
        }

        inline xmodule(const xhook::xmodule& other) noexcept : m_base{ other.m_base }
        {
        }

        template<class char_t>
        requires (std::is_same_v<char_t, char>    || std::is_same_v<char_t, char8_t> || 
                  std::is_same_v<char_t, wchar_t> || std::is_same_v<char_t, char16_t>)
        inline xmodule(const char_t* name) noexcept
        {
            this->operator=(name);
        }

        template<class char_t>
        requires (std::is_same_v<char_t, char>    || std::is_same_v<char_t, char8_t> || 
                  std::is_same_v<char_t, wchar_t> || std::is_same_v<char_t, char16_t>)
        inline auto operator=(const char_t* name) noexcept -> xhook::xmodule&
        {
            if constexpr (sizeof(char_t) == sizeof(char)) 
            {
                this->m_base = xhook::get_base(*reinterpret_cast<const char**>(&name));
            }
            else if constexpr(sizeof(char_t) == sizeof(wchar_t))
            {
                this->m_base = xhook::get_base(*reinterpret_cast<const wchar_t**>(&name));
            }
            return *this;
        }

        inline auto operator=(void* base) noexcept -> xhook::xmodule&
        {
            this->m_base = base;
            return *this;
        }

        inline auto operator=(const xhook::xmodule& other) noexcept -> xhook::xmodule&
        {
            this->m_base = other.m_base;
            return *this;
        }

        inline auto operator=(xhook::xmodule&& other) noexcept -> xhook::xmodule&
        {
            this->m_base = other.m_base;
            return *this;
        }

        inline auto base() const noexcept -> void*
        {
            return this->m_base;
        }

        template<xhook::is_function_pointer auto func>
        inline auto add_hook(xhook::is_rva auto rva, bool commit = true) const noexcept -> bool
        {
            if (rva.value != nullptr)
            {
                auto ptr{ reinterpret_cast<std::uintptr_t>(this->m_base) + reinterpret_cast<std::uintptr_t>(rva.value) };
                return xhook::hooker::add<func>(ptr, commit);
                
            }
            return false;
        }

        template<xhook::is_function_pointer auto func>
        inline auto add_hook(const char* symbol, bool commit = true) const noexcept -> bool
        {
            if (this->m_base != nullptr)
            {
                void* const ptr{ xhook::get_addr(this->m_base, symbol) };
                if (ptr != nullptr)
                {
                    return xhook::hooker::add<func>(ptr, commit);
                }
            }
            return false;
        }

        template<xhook::is_function_pointer auto func, class T> requires
        requires(T t) { { t.data() } -> std::convertible_to<const char*>; }
        inline auto add_hook(T&& symbol, bool commit = true) const noexcept -> bool
        {
            return this->add_hook<func>(reinterpret_cast<const char*>(symbol.data()), commit);
        }

        template<xhook::is_symbol_entry entry_t>
        inline auto add_hook(entry_t&& entry, bool commit = true) const noexcept 
            -> xhook::result<typename entry_t::type&, typename entry_t::type>
        {

            void **raw{}, *tar{};

            if constexpr (xhook::is_xhook_function<decltype(entry.function)>)
            {
                raw = reinterpret_cast<void**>(&entry.function.raw );
                tar = xhook::uniptr_t{ entry.function.get() }.ptr;
            }
            else /*if constexpr (xhook::is_function_pointer<decltype(entry.function)>)*/ 
            {
                raw = reinterpret_cast<void**>(&entry.function);
                tar = xhook::uniptr_t{ entry.function }.ptr;
            }

            if (this->m_base != nullptr)
            {
                void* const ptr{ xhook::get_addr(this->m_base, entry.symbol.data) };
                if (ptr != nullptr)
                {
                    *raw = ptr;
                    xhook::add_hook(raw, tar, commit);
                }
            }

            if constexpr (xhook::is_xhook_function<decltype(entry.function)>)
            {
                return
                {
                    reinterpret_cast<typename entry_t::type&>(entry.function.raw),
                    reinterpret_cast<typename entry_t::type> (entry.function.get())
                };
            }
            
            else /*if constexpr (xhook::is_function_pointer<decltype(entry.function)>)*/ 
            {
                return
                { 
                     reinterpret_cast<typename entry_t::type&>(entry.function),
                    *reinterpret_cast<typename entry_t::type*>(&tar)
                };
            }
        }

        template<xhook::is_rva_hook_entry entry_t>
        inline auto add_hook(entry_t&& entry, bool commit = true) const noexcept 
            -> xhook::result<xhook::fptr_t<entry_t>&, xhook::fptr_t<entry_t>>
        {
            void** raw{}, *tar{};

            if constexpr (xhook::is_pointer_entry<entry_t>) 
            {
                if constexpr (xhook::is_xhook_function<decltype(entry.function)>) 
                {
                    raw = reinterpret_cast<void**>(&entry.function.raw);
                    tar = xhook::uniptr_t{ entry.function.get() }.ptr;
                }
                else /*if constexpr (xhook::is_function_pointer<decltype(entry.function)>)*/
                {
                    raw = reinterpret_cast<void**>(&entry.function);
                    tar = xhook::uniptr_t{ entry.function }.ptr;
                }

                *reinterpret_cast<std::uintptr_t*>(raw) = std::uintptr_t
                {
                    reinterpret_cast<std::uintptr_t>(this->m_base) +
                    reinterpret_cast<std::uintptr_t>(entry.pointer.value)
                };
            }
            else if constexpr(xhook::is_fixed_function_pointer_entry<entry_t>)
            {
                using target = typename entry_t::target_type;
                raw = reinterpret_cast<void**>(&target::call);
                tar = xhook::uniptr_t{ entry_t::func }.ptr;

                *reinterpret_cast<std::uintptr_t*>(raw) = std::uintptr_t
                {
                    reinterpret_cast<std::uintptr_t>(this->m_base) +
                    reinterpret_cast<std::uintptr_t>(entry.ptr)
                };
            }

            if (this->m_base != nullptr)
            {
                xhook::add_hook(raw, tar, commit);
            }

            if constexpr (xhook::is_pointer_entry<entry_t>)
            {
                if constexpr (xhook::is_xhook_function<decltype(entry.function)>) 
                {
                    using function = std::remove_reference_t<decltype(entry.function)>;
                    return
                    {
                        reinterpret_cast<xhook::fptr_t<entry_t>&>(entry.function.raw  ),
                        reinterpret_cast<xhook::fptr_t<entry_t>> (entry.function.get())
                    };
                }
                else /*if constexpr (xhook::is_function_pointer<decltype(entry.function)>)*/
                {
                    return
                    {
                         reinterpret_cast<xhook::fptr_t<entry_t>&>(&entry.function),
                        *reinterpret_cast<xhook::fptr_t<entry_t>*>(&tar)
                    };
                }
            }
            else /*if constexpr (xhook::is_fixed_function_pointer_entry<entry_t>)*/
            {
                using target = typename entry_t::target_type;
                return
                {
                     reinterpret_cast<xhook::fptr_t<entry_t>&>(target::call),
                    *reinterpret_cast<xhook::fptr_t<entry_t>*>(&tar)
                };
            }
        }
        
        template<xhook::is_symbol_entry entry_t>
        auto operator&(entry_t entry) const noexcept -> xhook::result<typename entry_t::type&, typename entry_t::type>
        {
            return this->add_hook(std::move(entry), true);
        }

        template<xhook::is_rva_hook_entry entry_t>
        auto operator&(entry_t entry) const noexcept -> xhook::result<xhook::fptr_t<entry_t>&, xhook::fptr_t<entry_t>>
        {
            return this->add_hook(std::move(entry), true);
        }
        
        template<xhook::is_symbol_entry entry_t>
        auto operator&=(entry_t entry) const noexcept -> xhook::result<typename entry_t::type&, typename entry_t::type>
        {
            return this->add_hook(std::move(entry), true);
        }

        template<xhook::is_rva_hook_entry entry_t>
        auto operator&=(entry_t entry) const noexcept -> xhook::result<xhook::fptr_t<entry_t>&, xhook::fptr_t<entry_t>>
        {
            return this->add_hook(std::move(entry), true);
        }

        template<class entry_t>
        requires (xhook::is_rva_hook_entry<entry_t> || xhook::is_symbol_entry<entry_t>)
        auto operator+=(entry_t entry) noexcept -> xhook::xmodule&
        {
            this->add_hook(std::move(entry), true);
            return *this;
        }

        template<class entry_t>
        requires (xhook::is_rva_hook_entry<entry_t> || xhook::is_symbol_entry<entry_t>)
        auto operator+=(entry_t entry) const noexcept -> const xhook::xmodule&
        {
            this->add_hook(std::move(entry), true);
            return *this;
        }

    };

    template <class char_t, size_t N>
    requires ((sizeof(char_t) == sizeof(char) || 
               sizeof(char_t) == sizeof(wchar_t)) && (N >1))
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

    namespace dll
    {
        template <xhook::string_dll name_dll>
        constexpr auto operator""_dll() noexcept -> xhook::xmodule
        {
            if constexpr (sizeof(decltype(name_dll.data[0])) == sizeof(char))
            {
                return reinterpret_cast<const char*>(name_dll.data);
            }
            else
            {
                return reinterpret_cast<const wchar_t*>(name_dll.data);
            }
        }
    }
}
/*
    c++委托库
    用法示例:
        Delegate<void,const int&> del;          //声明一个委托
        std::vector<int> v;
        del += {v, &decltype(v)::push_back};    //添加变量 v 的 push_back 方法
        del(1);                                 //调用委托，传入参数 1
                                                // v 中成功添加整数 1

    其他:
        1、为防止调用空委托，可以使用 TryInvoke 方法。
           TryInvoke 方法封装了对委托是否为空的判断，通过自身返回值确定是否调
           用成功（即是否不为空）。若需要委托的返回值，则应该使用 TryInvoke
           的重载版本，在第一个参数前面额外传入要接受返回值的变量。
        2、若不需要多播委托，则可以直接使用 Delegate_Single 类，该类型只支持
           存储一个委托。
        3、Empty 类和 Empty_vbptr 类仅作为强制类型转换时的中间类型，无实际意义。
        4、若意外调用了空委托，则会抛出 MyCodes::bad_invoke 类型的异常。
        5、如果要绑定的函数有重载版本，那么用类似 del += {v, &decltype(v)::push_back};
           的语法时，编译器无法推导出是哪个重载，此时应该用 Add 方法，即
           del.Add(v,&decltype(v)::push_back);  Sub 方法同理。
        6、对返回值为 void 的委托做了特殊处理，可以存储返回为任意类型的单委托，但是
           返回值会被舍弃。使用这种委托时，由于模板中又多了两个参数类型，编译器在
           少数的某些时候（如绑定 cout 的 operator<< 方法时），无法做出正确的推导
           （虽然 Intellicense 在编辑代码的时候可以正确推导，但是编译时有可能推导失败），
           此时就需要显式地声明一个 Delegate_Single_any 类型临时变量，调用 Bind
           方法将要绑定的对象绑定到该临时变量中（要显式指定模板参数），然后将该对象添加
           到委托中。
                例：
                    Delegate<void,size_t> del;
                    Delegate_Single_any<void, size_t> temp;
                    temp.Bind<ostream&, ostream>(cout, &ostream::operator<<);
                    del += temp;
        7、Delegate_view 类是对委托进行的包装，内部存储一个委托的指针，对外提供
           的方法中不包含调用委托的方法，只能注册或删除委托。在类中，如果不希望
           其他对象调用自身的委托，则可以把委托作为私有成员或保护成员，然后对外提
           供一个指向该委托的委托视图。
                用法示例:
                class CLS
                {
                public:
                    Delegate_view<void> del_view = del;
                private:
                    Delegate<void> del;
                };
*/
#pragma once
#include<vector>
#include<exception>
#pragma warning(disable:6011)	
#pragma warning(disable:6101)   //让编译器不要发出空指针警告和未初始化_Out_参数警告
#ifndef delegate
    #define delegate Delegate
    #pragma message("delegate 已被定义为 Delegate")
#endif // !delegate
#pragma message("delegate 已被定义为 Delegate")
#if _MSVC_LANG > 201703L
    #define mycodes_delegate_cpp20 1
#else
    #define mycodes_delegate_cpp20 0
#endif

namespace MyCodes
{
    class bad_invoke :public std::exception
    {
    public:
        bad_invoke():std::exception("试图调用空委托获得返回值")
        {

        }
    };

    template<class DelType>
    inline bool subDelegate(const DelType& del, std::vector<DelType>& allDels)noexcept
    {//使用反向迭代器,把最后面的一个满足条件的委托移除
        for (auto it = allDels.rbegin(); it != allDels.rend(); it++)
        {
            if (*it == del)
            {
                allDels.erase(--(it.base()));
                return true;
            }
        }
        return false;
    }

    template<class DelType>
    inline bool haveDelegate(const DelType& del, const std::vector<DelType>& allDels)noexcept
    {
        for (const auto& _del : allDels)
        {
            if (del == _del)
            {
                return true;
            }
        }

        return false;
    }
}

namespace MyCodes
{
    class Empty	//空类型
    {

    };

#if mycodes_delegate_cpp20
    //带有 vbptr 的空类型
    class Empty_vbptr :virtual Empty
    {

    };

    class Empty1
    {

    };

    //多继承的空类型
    class Empty_multiple :Empty, Empty1
    {

    };

    template<class T>
    concept is_virtual_override = requires(void(T:: * fp)())
    {
        reinterpret_cast<void(Empty_vbptr::*)()>(fp);
    };

    template<class T>
    concept is_multiple_override = requires(void(T:: * fp)())
    {
        reinterpret_cast<void(Empty_multiple::*)()>(fp);
    };

    template<class Lambda,class Ty_ret,class...Ty_params>
    concept is_lambda = requires(Lambda lam,Ty_params... params)
    {
        (Ty_ret)lam(params...);
    };

    template<class Lambda, class...Ty_params>
    concept is_lambda_any = requires(Lambda lam, Ty_params... params)
    {
        lam(params...);
    };
#endif
}

namespace MyCodes
{
#if mycodes_delegate_cpp20
    enum class CallType
    {
        null, this_call, static_call, vbptr_this_call, multiple_this_call
    };

    template<class Ty_ret, class... Ty_params>
    class Delegate_Single	//单委托
    {
        template<class, class...Ty_params>
        friend class Delegate_Single_any;
    public:
        Delegate_Single() = default;
        Delegate_Single(const Delegate_Single&) = default;
        template<class CLS>
        Delegate_Single(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))
        {
            this->Bind(__this, __fun);
        }
        template<class CLS>
        Delegate_Single(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)
        {
            this->Bind(__this, __fun);
        }
        Delegate_Single(Ty_ret(*__fun)(Ty_params...))
        {
            this->Bind(__fun);
        }
        template<class Lambda>
        requires is_lambda<Lambda,Ty_ret,Ty_params...>
        Delegate_Single(const Lambda& lam)
        {
            this->Bind(lam);
        }        

        //绑定类对象和类成员函数
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty::*)()))
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::this_call;
            _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty::*)()))
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::this_call;
            _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_vbptr::*)()))
        &&is_virtual_override<CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_vbptr::*)()))
        && is_virtual_override<CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_multiple::*)()))
        &&is_multiple_override<CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::multiple_this_call;
            _this._ptr_multiple = reinterpret_cast<decltype(_this._ptr_multiple)>(const_cast<CLS*>(&__this));
            _fun._this_fun_multiple = reinterpret_cast<decltype(_fun._this_fun_multiple)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_multiple::*)()))
        &&is_multiple_override<CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::multiple_this_call;
            _this._ptr_multiple = reinterpret_cast<decltype(_this._ptr_multiple)>(const_cast<CLS*>(&__this));
            _fun._this_fun_multiple = reinterpret_cast<decltype(_fun._this_fun_multiple)> (__fun);
        }
        //绑定静态函数
        void Bind(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            _call_type = CallType::static_call;
            _this.value = nullptr;
            _fun.static_fun = __fun;
        }
        //绑定 lambda
        template<class Lambda>
        requires is_lambda<Lambda,Ty_ret,Ty_params...>
        __forceinline void Bind(const Lambda& lam)
        {
            if constexpr (is_empty_v<Lambda>)
            {
                this->Bind(static_cast<Ty_ret(*)(Ty_params...)>(lam));
            }
            else
            {
                this->Bind(lam, &Lambda::operator());
            }
        }

        bool IsNull()const noexcept
        {
            return _call_type == CallType::null;
        }
        operator bool()const noexcept
        {
            return _call_type != CallType::null;
        }

        //触发调用
        Ty_ret Invoke(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            switch (_call_type)
            {
            case CallType::static_call:
            {
                return _fun.static_fun(params...);
            }
            break;
            case CallType::this_call:
            {
                return (_this._ptr->*(_fun.this_fun))(params...);
            }
            break;
            case CallType::vbptr_this_call:
            {
                return (_this._ptr_vbptr->*(_fun._this_fun_vbptr))(params...);
            }
            break;
            case CallType::multiple_this_call:
            {
                return (_this._ptr_multiple->*(_fun._this_fun_multiple))(params...);
            }
            break;
            default:
                break;
            }

            //如果委托不能调用
            if constexpr (!is_void_v<Ty_ret>)
            {
                throw bad_invoke();
            }
            else
            {
                return;
            }
        }
        Ty_ret operator()(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            return Invoke(params...);
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            if (this->IsNull())
                return false;
            else
            {
                Invoke(params...);
                return true;
            }
        }

        //解除绑定
        void UnBind()noexcept
        {
            _this.value = nullptr;
            _fun._this_fun_vbptr = nullptr;
            _call_type = CallType::null;
        }

        bool operator==(const Delegate_Single& right)const noexcept
        {
            switch (_call_type)
            {
            case CallType::static_call:
                return this->_fun.static_fun == right._fun.static_fun;
            case CallType::this_call:
                return this->_this.value == right._this.value &&
                    this->_fun.this_fun == right._fun.this_fun;
            case CallType::vbptr_this_call:
                return this->_this.value == right._this.value &&
                    this->_fun._this_fun_vbptr == right._fun._this_fun_vbptr;
            case CallType::multiple_this_call:
                return this->_this.value == right._this.value &&
                    this->_fun._this_fun_multiple == right._fun._this_fun_multiple;
            default:
                break;
            }

            return true;
        }
        const Delegate_Single& operator=(const Delegate_Single& right)noexcept
        {
            this->_call_type = right._call_type;
            this->_this = right._this;
            this->_fun = right._fun;
            return *this;
        }

    protected:
        union ThisPtr
        {
            void* value = nullptr;
            Empty* _ptr;
            Empty_vbptr* _ptr_vbptr;
            Empty_multiple* _ptr_multiple;
        };
        union CallFun
        {
            void* value;
            Ty_ret(Empty::* this_fun)(Ty_params...);
            Ty_ret(*static_fun)(Ty_params...);
            Ty_ret(Empty_vbptr::* _this_fun_vbptr)(Ty_params...) = nullptr;
            Ty_ret(Empty_multiple::* _this_fun_multiple)(Ty_params...);
        };

        CallType _call_type = CallType::null;
        ThisPtr _this;
        CallFun _fun;
    };
#else
    template<class Ty_ret, class... Ty_params>
    class Delegate_Single	//单委托
    {
        template<class, class...Ty_params>
        friend class Delegate_Single_any;
    public:
        Delegate_Single() = default;
        Delegate_Single(const Delegate_Single&) = default;
        template<class CLS>
        Delegate_Single(const CLS& __this, Ty_ret(CLS::* __fun)(const Ty_params...))
        {
            this->Bind(__this, __fun);
        }
        template<class CLS>
        Delegate_Single(const CLS& __this, Ty_ret(CLS::* __fun)(const Ty_params...)const)
        {
            this->Bind(__this, __fun);
        }
        Delegate_Single(Ty_ret(*__fun)(const Ty_params...))
        {
            this->Bind(__fun);
        }
        template<class Lambda>
        Delegate_Single(const Lambda& lam)
        {
            this->Bind(lam);
        }

        //绑定类对象和类成员函数
        template<class CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(const Ty_params...))noexcept
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(const Ty_params...)const)noexcept
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        //绑定静态函数
        void Bind(Ty_ret(*__fun)(const Ty_params...))noexcept
        {
            _this = nullptr;
            _fun.static_fun = __fun;
        }
        //绑定 lambda
        template<class Lambda>
        __forceinline void Bind(const Lambda& lam)
        {
            if constexpr (is_empty_v<Lambda>)
            {
                this->Bind(static_cast<Ty_ret(*)(Ty_params...)>(lam));
            }
            else
            {
                this->Bind(lam, &Lambda::operator());
            }
        }

        bool IsNull()const noexcept
        {
            return _fun.isNull();
        }
        operator bool()const noexcept
        {
            return !_fun.isNull();
        }

        //触发调用
        Ty_ret Invoke(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            if (IsNull())
            {
                if constexpr (is_void_v<Ty_ret>)
                    return;
                else
                    throw bad_invoke();
            }


            if (_this)
                return (_this->*(_fun.this_fun))(params...);
            else
                return _fun.static_fun(params...);
        }
        Ty_ret operator()(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            return Invoke(params...);
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            if (IsNull())
                return false;
            else
            {
                Invoke(params...);
                return true;
            }
        }

        //解除绑定
        void UnBind()noexcept
        {
            _this = nullptr;
            _fun.value = nullptr;
        }

        bool operator==(const Delegate_Single& right)const noexcept
        {
            return ((this->_fun.value == right._fun.value) &&
                this->_this == right._this);
        }
        const Delegate_Single& operator=(const Delegate_Single& right)noexcept
        {
            this->_this = right._this;
            this->_fun = right._fun;
            return *this;
        }

    protected:
        union _CallFun
        {
            void* value = nullptr;
            Ty_ret(Empty::* this_fun)(const Ty_params...);
            Ty_ret(*static_fun)(const Ty_params...);
            __forceinline bool isNull()const noexcept
            {
                return this->value == nullptr;
            }
        };

        Empty* _this = nullptr;
        _CallFun _fun;
    };
#endif

    template<template<class ret,class...params>class _Delegate_Single,
        class Ty_ret, class... Ty_params>
    class Delegate_base //多播委托基类
    {
    public:
        using Delegate_Single_Type = _Delegate_Single<Ty_ret, Ty_params...>;
        //添加委托
        template<class CLS>
        __forceinline void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class CLS>
        __forceinline void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        //添加静态委托
        void Add(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__fun);
            m_allDels.push_back(temp);
        }
        void Add(const Delegate_Single_Type& del)noexcept
        {
            if (!del.IsNull())
                this->m_allDels.push_back(del);
        }
        //添加lambda
        template<class Lambda>
#if mycodes_delegate_cpp20
        requires is_lambda<Lambda,Ty_ret,Ty_params...>
#endif
        void Add(const Lambda& lam)
        {
            Delegate_Single_Type temp;
            temp.Bind(lam);
            this->m_allDels.push_back(temp);
        }
        Delegate_base& operator+=(const Delegate_Single_Type& del)noexcept
        {
            if (!del.IsNull())
                this->m_allDels.push_back(del);
            return *this;
        }

        void Clear() noexcept
        {
            m_allDels.clear();
        }
        bool Empty()const noexcept
        {
            return m_allDels.size() == 0;
        }
        operator bool()const noexcept
        {
            return !Empty();
        }
        const std::vector<Delegate_Single_Type>& GetArray()const noexcept
        {
            return this->m_allDels;
        }
        _declspec(property(get = getsize)) const size_t size;
        const size_t getsize()const noexcept
        {
            return m_allDels.size();
        }

        //触发调用
        Ty_ret Invoke(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            for (size_t i = 0; i < m_allDels.size(); i++)
            {
                if constexpr (!is_void_v<Ty_ret>)
                    if (i == m_allDels.size() - 1)
                    {
                        return m_allDels[i].Invoke(params...);
                    }

                m_allDels[i].Invoke(params...);
            }

            if constexpr (!is_void_v<Ty_ret>)
                throw bad_invoke();
        }
        Ty_ret operator()(const Ty_params&... params)const noexcept(is_void_v<Ty_ret>)
        {
            return Invoke(params...);
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            if (Empty())
            {
                return false;
            }
            else
            {
                Invoke(params...);
                return true;
            }
        }

        //删除委托
        template<class CLS>
        __forceinline bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class CLS>
        __forceinline bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        //删除静态委托
        bool Sub(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__fun);
            return subDelegate(temp, m_allDels);
        }
        bool Sub(const Delegate_Single_Type& del)noexcept
        {
            return subDelegate(del, m_allDels);
        }
        bool operator-=(const Delegate_Single_Type& del)noexcept
        {
            return subDelegate(del, m_allDels);
        }

        template<class CLS>
        __forceinline bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))const noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class CLS>
        __forceinline bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)const noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(Ty_ret(*__fun)(Ty_params...))const noexcept
        {
            Delegate_Single_Type temp;
            temp.Bind(__fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(const Delegate_Single_Type& del)const noexcept
        {
            return haveDelegate(del, m_allDels);
        }

    protected:
        Delegate_base() = default;
        Delegate_base(const Delegate_base&) = default;
        Delegate_base(Delegate_base&&) = default;
        std::vector<Delegate_Single_Type> m_allDels;
    };

    template<class Ty_ret, class... Ty_params>
    class Delegate :public Delegate_base<Delegate_Single,Ty_ret, Ty_params...>
    {
    public:
        bool TryInvoke(_Out_ Ty_ret& out, _In_ const Ty_params&... params)const noexcept
        {
            if (this->Empty())
            {
                return false;
            }
            else
            {
                out = this->Invoke(params...);
                return true;
            }
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            return Delegate_base<Ty_ret, Ty_params...>::TryInvoke(params...);
        }
    };

    template<class Ty_ret, class...Ty_params>
    class Delegate<const Ty_ret&, Ty_params...> :public Delegate_base<Delegate_Single,const Ty_ret&, Ty_params...>
    {
    public:
        bool TryInvoke(_Out_ Ty_ret& out, _In_ const Ty_params&... params)const noexcept
        {
            if (this->Empty())
            {
                return false;
            }
            else
            {
                out = this->Invoke(params...);
                return true;
            }
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            return Delegate_base<Ty_ret, Ty_params...>::TryInvoke(params...);
        }
    };

    template<class Ty_ret, class...Ty_params>
    class Delegate<const Ty_ret, Ty_params...> :public Delegate_base<Delegate_Single,const Ty_ret, Ty_params...>
    {
    public:
        bool TryInvoke(_Out_ Ty_ret& out, _In_ const Ty_params&... params)const noexcept
        {
            if (this->Empty())
            {
                return false;
            }
            else
            {
                out = this->Invoke(params...);
                return true;
            }
        }
        bool TryInvoke(const Ty_params&... params)const noexcept
        {
            return Delegate_base<Ty_ret, Ty_params...>::TryInvoke(params...);
        }
    };
}

namespace MyCodes
{
    /*  返回值为 void 时，特化的单委托类型。作为返回值为 void 类型的多播委托中的存储元素
      这种特化版本的单委托可以存储任意返回值的方法，但返回值会被舍弃。
      原理是该委托中存储了两个一般的单委托，分为顶层委托和底层委托两部分，底层委托以二进制方
    式存储实际指向的对象和方法，顶层委托则指向底层委托的 TryInvoke 方法。调用这种特化的委托
    时，由顶层委托调用底层委托，底层委托调用完毕后，返回值会被抛弃，然后顶层委托调用结束，返
    回 void。*/
    template<class _Ty_ret,class...Ty_params>
    class Delegate_Single_any
    {
        static_assert(is_void_v<_Ty_ret>, "Delegate_Single_any模板的第一个参数应为void");
    protected:
        using Byte = unsigned char;
        static constexpr size_t del_size = sizeof(Delegate_Single<void>);
        Delegate_Single<bool,const Ty_params&...> top_del;
        Byte bottom_del[del_size]{0};
    public:
        Delegate_Single_any() = default;
        Delegate_Single_any(const Delegate_Single_any& right)
        {
            this->operator=(right);
        }
        template<class Ty_ret,class CLS>
        Delegate_Single_any(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            this->Bind(__this, __fun);
        }
        template<class Ty_ret,class CLS>
        Delegate_Single_any(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            this->Bind(__this, __fun);
        }
        template<class Ty_ret>
        Delegate_Single_any(Ty_ret(* __fun)(Ty_params...))noexcept
        {
            this->Bind(__fun);
        }
        template<class Lambda>
#if mycodes_delegate_cpp20
            requires is_lambda_any<Lambda, Ty_params...>
#endif
        Delegate_Single_any(const Lambda& lam)
        {
            this->Bind(lam);
        }

        template<class Ty_ret,class CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            Delegate_Single<Ty_ret, Ty_params...>* bottom_del_ptr = 
                reinterpret_cast<Delegate_Single<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &Delegate_Single<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret,class CLS>
        __forceinline void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            Delegate_Single<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<Delegate_Single<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &Delegate_Single<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret>
        __forceinline void Bind(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            Delegate_Single<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<Delegate_Single<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__fun);
            top_del.Bind(*bottom_del_ptr, &Delegate_Single<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Lambda>
#if mycodes_delegate_cpp20
        requires is_lambda_any<Lambda,Ty_params...>
#endif
        __forceinline void Bind(const Lambda& lam)
        {
            using Delegate_Single_t = decltype(Delegate_Single{lam,&Lambda::operator() });
            Delegate_Single_t* bottom_del_ptr =
                reinterpret_cast<Delegate_Single_t*>(bottom_del);
            bottom_del_ptr->Bind(lam);
            top_del.Bind(*bottom_del_ptr, &Delegate_Single_t::TryInvoke);
        }

        void UnBind()
        {
            Delegate_Single<void>* bottom_del_ptr =
                reinterpret_cast<Delegate_Single<void>*>(bottom_del);
            bottom_del_ptr->    UnBind();
            top_del.            UnBind();
        }

        void Invoke(const Ty_params&...params)const noexcept
        {
            if (top_del)
                top_del(params...);
        }
        void operator()(const Ty_params&...params)const noexcept
        {
            Invoke(params...);
        }

        bool operator==(const Delegate_Single_any& right)const noexcept
        {
            const Delegate_Single<void>* __this =
                reinterpret_cast<const Delegate_Single<void>*>(this->bottom_del);
            const Delegate_Single<void>* __right =
                reinterpret_cast<const Delegate_Single<void>*>(right.bottom_del);
            return *__this == *__right;
        }
        const Delegate_Single_any& operator=(const Delegate_Single_any& right)noexcept
        {
            if (right.IsNull())
                this->UnBind();

            Delegate_Single<void>* __this =
                reinterpret_cast<Delegate_Single<void>*>(this->bottom_del);
            const Delegate_Single<void>* __right =
                reinterpret_cast<const Delegate_Single<void>*>(right.bottom_del);

            *__this = *__right;
            this->top_del = right.top_del;
#if mycodes_delegate_cpp20
            this->top_del._this.value =this->bottom_del;
#else
            this->top_del._this =reinterpret_cast<decltype(this->top_del._this)>(this->bottom_del);
#endif
            return *this;
        }
        bool IsNull()const noexcept
        {
            return top_del.IsNull();
        }
        operator bool()const noexcept
        {
            return (bool)top_del;
        }
    };

    template<class...Ty_params>
    class Delegate<void, Ty_params...> :public Delegate_base<Delegate_Single_any,void,Ty_params...>
    {

    };
}

namespace MyCodes
{
    template<class Ty_ret, class...Ty_params>
    class Delegate_view //委托视图
    {
        using Delegate_Single = typename Delegate<Ty_ret, Ty_params...>::Delegate_Single_Type;
    public:
        Delegate_view(Delegate<Ty_ret, Ty_params...>& del)
        {
            m_del = &del;
        }

        void Add(const Delegate_Single& del)noexcept
        {
            m_del->Add(del);
        }
        Delegate_view& operator+=(const Delegate_Single& del)noexcept
        {
            m_del->operator+=(del);
            return *this;
        }

        void Sub(const Delegate_Single& del)noexcept
        {
            m_del->Sub(del);
        }
        Delegate_view& operator-=(const Delegate_Single& del)noexcept
        {
            m_del->operator-=(del);
            return *this;
        }

        bool Have(const Delegate_Single& del)const noexcept
        {
            return m_del->Have(del);
        }

        size_t size()const noexcept
        {
            return m_del->size;
        }
        bool Empty()const noexcept
        {
            return m_del->Empty();
        }
    protected:
        void operator=(const Delegate_view&) = delete;

        Delegate<Ty_ret, Ty_params...>* m_del;
    };

    template<class Ty_ret, class...Ty_params>
    using Event = Delegate<Ty_ret, Ty_params...>;
    template<class Ty_ret, class...Ty_params>
    using Event_view = Delegate_view<Ty_ret, Ty_params...>;
}

#pragma warning(default:6011)
#pragma warning(default:6101)

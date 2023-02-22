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
        2、若不需要多播委托，则可以直接使用 DelegateSingle 类，该类型只支持
           存储一个委托。
        3、空类型仅作为强制类型转换时的中间类型，无实际意义。
        4、若意外调用了空委托，则会抛出 MyCodes::bad_invoke 类型的异常。
        5、如果要绑定的函数有重载版本，那么用类似 del += {v, &decltype(v)::push_back};
           的语法时，编译器无法推导出是哪个重载，此时应该用 Add 方法，即
           del.Add(v,&decltype(v)::push_back);  Sub 方法同理。
        6、对返回值为 void 的委托做了特殊处理，可以存储返回为任意类型的单委托，但是
           返回值会被舍弃。使用这种委托时，由于模板中又多了两个参数类型，编译器在
           少数的某些时候（如绑定 cout 的 operator<< 方法时），无法做出正确的推导
           （虽然 Intellicense 在编辑代码的时候可以正确推导，但是编译时有可能推导失败），
           此时就需要显式地声明一个 DelegateSingle_any 类型临时变量，调用 Bind
           方法将要绑定的对象绑定到该临时变量中（要显式指定模板参数），然后将该对象添加
           到委托中。
                例：
                    Delegate<void,size_t> del;
                    DelegateSingle_any<void, size_t> temp;
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
#if _MSVC_LANG < 201402L
#error(delegate.hpp：请使用c++14及以上的版本)
#endif
#if _MSVC_LANG > 201703L    //判断当前项目c++语言版本是否支持c++20
    #define mycodes_delegate_cpp20 1
#else
    #define mycodes_delegate_cpp20 0
#endif

#pragma push_macro("IF_CONSTEXPR")
#undef IF_CONSTEXPR
#pragma push_macro("CONSTEXPR")
#undef CONSTEXPR

#if _MSVC_LANG > 201402L    //判断当前项目c++语言版本是否支持c++17
    #define mycodes_delegate_cpp17 1
    #define IF_CONSTEXPR if constexpr
    #define CONSTEXPR constexpr
#else
    #define mycodes_delegate_cpp17 0
    #define IF_CONSTEXPR if
    #define CONSTEXPR const
#endif

namespace MyCodes
{//定义了一些下面通用的东西
    class bad_invoke :public std::exception
    {
    public:
        bad_invoke():std::exception("试图调用空委托获得返回值")
        {

        }
    };

    enum class CallType //单个委托的调用方式
    {
        null, this_call, static_call, vbptr_this_call, multiple_this_call
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
    //定义了一些转换并存储委托指针用的空类型

    class Empty	//空类型
    {

    };

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

#if mycodes_delegate_cpp20  
    template<class T>
    concept is_virtual_override = requires(void(T:: * fp)())
    {
        //判断是否是带有vbptr的类
        reinterpret_cast<void(Empty_vbptr::*)()>(fp);
    };

    template<class T>
    concept is_multiple_override = requires(void(T:: * fp)())
    {
        //判断是否是多继承的类
        reinterpret_cast<void(Empty_multiple::*)()>(fp);
    };

    template<class Lambda,class Ty_ret,class...Ty_params>
    concept is_lambda = requires(Lambda lam,Ty_params... params)
    {
        //判断是否是特定参数和返回值的可调用对象
        (Ty_ret)lam(params...);
    };

    template<class Lambda, class...Ty_params>
    concept is_lambda_any = requires(Lambda lam, Ty_params... params)
    {
        //判断是否是特定参数的可调用对象
        lam(params...);
    };
#endif
}

namespace MyCodes
{
    template<class Ty_ret, class... Ty_params>
    class DelegateSingle	//单委托
    {
        template<class, class...Ty_params>
        friend class DelegateSingle_any;
    public:
        DelegateSingle() = default;
        DelegateSingle(const DelegateSingle&) = default;
        template<class CLS>
        DelegateSingle(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))
        {
            this->Bind(__this, __fun);
        }
        template<class CLS>
        DelegateSingle(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)
        {
            this->Bind(__this, __fun);
        }
        DelegateSingle(Ty_ret(*__fun)(Ty_params...))
        {
            this->Bind(__fun);
        }
        template<class Lambda>
        #if mycodes_delegate_cpp20
        requires is_lambda<Lambda,Ty_ret,Ty_params...>
        #endif
        DelegateSingle(const Lambda& lam)
        {
            this->Bind(lam);
        }        

#if mycodes_delegate_cpp20  //如果支持c++20，那么就可以只用一个Bind函数就重载出所有不同的绑定方式
        //绑定类对象和类成员函数
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty::*)()))
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::this_call;
            _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty::*)()))
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::this_call;
            _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_vbptr::*)()))
        &&is_virtual_override<CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_vbptr::*)()))
        && is_virtual_override<CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_multiple::*)()))
        &&is_multiple_override<CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::multiple_this_call;
            _this._ptr_multiple = reinterpret_cast<decltype(_this._ptr_multiple)>(const_cast<CLS*>(&__this));
            _fun._this_fun_multiple = reinterpret_cast<decltype(_fun._this_fun_multiple)> (__fun);
        }
        template<class CLS>
            requires (sizeof(void(CLS::*)()) == sizeof(void(Empty_multiple::*)()))
        &&is_multiple_override<CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
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
        void Bind(const Lambda& lam)
        {
            IF_CONSTEXPR (std::is_empty_v<Lambda>)
            {
                this->Bind(static_cast<Ty_ret(*)(Ty_params...)>(lam));
            }
            else
            {
                this->Bind(lam, &Lambda::operator());
            }
        }

#else
        //绑定类对象和类成员函数
        template<class CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            IF_CONSTEXPR(sizeof(__fun) == sizeof(void*))
            {
                _call_type = CallType::this_call;
                _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
                _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
            }
            #if mycodes_delegate_cpp17
            else
            {   //暂时不知道怎么用表达式区分带vbptr的类型和多继承的类
                //若要绑定多继承的类对象，需要用 Bind_multiple 函数绑定
                this->Bind_vbptr(__this, __fun);
            }
            #endif
        }
        template<class CLS>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            IF_CONSTEXPR(sizeof(__fun) == sizeof(void*))
            {
                _call_type = CallType::this_call;
                _this._ptr = reinterpret_cast<decltype(_this._ptr)>(const_cast<CLS*>(&__this));
                _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
            }
            #if mycodes_delegate_cpp17
            else
            {   //暂时不知道怎么用表达式区分带vbptr的类型和多继承的类
                //若要绑定多继承的类对象，需要用 Bind_multiple 函数绑定
                this->Bind_vbptr(__this, __fun);
            }
            #endif
        }
        template<class CLS>
        void Bind_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
        void Bind_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            _call_type = CallType::vbptr_this_call;
            _this._ptr_vbptr = reinterpret_cast<decltype(_this._ptr_vbptr)>(const_cast<CLS*>(&__this));
            _fun._this_fun_vbptr = reinterpret_cast<decltype(_fun._this_fun_vbptr)> (__fun);
        }
        template<class CLS>
        void Bind_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            _call_type = CallType::multiple_this_call;
            _this._ptr_multiple = reinterpret_cast<decltype(_this._ptr_multiple)>(const_cast<CLS*>(&__this));
            _fun._this_fun_multiple = reinterpret_cast<decltype(_fun._this_fun_multiple)> (__fun);
        }
        template<class CLS>
        void Bind_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
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
        void Bind(const Lambda& lam)
        {
            #if mycodes_delegate_cpp17
            IF_CONSTEXPR(std::is_empty_v<Lambda>)
            {
                this->Bind(static_cast<Ty_ret(*)(Ty_params...)>(lam));
            }
            else
            #endif
            {
                this->Bind(lam, &Lambda::operator());
            }
        }
#endif

        bool IsNull()const noexcept
        {
            return _call_type == CallType::null;
        }
        operator bool()const noexcept
        {
            return _call_type != CallType::null;
        }

        //触发调用
        Ty_ret Invoke(const Ty_params&... params)const
        #if mycodes_delegate_cpp17
            noexcept(std::is_void_v<Ty_ret>)    
        #endif
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

            #if mycodes_delegate_cpp17
            //如果委托不能调用
            if constexpr (!std::is_void_v<Ty_ret>)
            {
                throw bad_invoke();
            }
            else
            {
                return;
            }
            #else
            throw bad_invoke();
            #endif
        }
        Ty_ret operator()(const Ty_params&... params)const 
        #if mycodes_delegate_cpp17
            noexcept(std::is_void_v<Ty_ret>)
        #endif
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
            _fun.dvalue[0] = nullptr;   _fun.dvalue[1] = nullptr;
            _call_type = CallType::null;
        }

        bool operator==(const DelegateSingle& right)const noexcept
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
            case CallType::null:
                return right.IsNull();
            default:
                break;
            }

            return false;
        }
        const DelegateSingle& operator=(const DelegateSingle& right)noexcept
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
            void* dvalue[2]{ nullptr,nullptr };
            Ty_ret(Empty_vbptr::* _this_fun_vbptr)(Ty_params...);
            Ty_ret(Empty_multiple::* _this_fun_multiple)(Ty_params...);
        };

        CallType _call_type = CallType::null;
        ThisPtr _this;
        CallFun _fun;
    };

    template<template<class ret,class...params>class _DelegateSingle,
        class Ty_ret, class... Ty_params>
    class Delegate_base //多播委托基类
    {
    public:
        using DelegateSingle_Type = _DelegateSingle<Ty_ret, Ty_params...>;
        //添加委托
        template<class CLS>
        void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class CLS>
        void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        //添加静态委托
        void Add(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            m_allDels.push_back(temp);
        }
        void Add(const DelegateSingle_Type& del)noexcept
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
            DelegateSingle_Type temp;
            temp.Bind(lam);
            this->m_allDels.push_back(temp);
        }
        Delegate_base& operator+=(const DelegateSingle_Type& del)noexcept
        {
            if (!del.IsNull())
                this->m_allDels.push_back(del);
            return *this;
        }

#if !mycodes_delegate_cpp20 //如果不支持c++20，就需要通过更多不同的函数来绑定不同的委托类型
        template<class CLS>
        void Add_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class CLS>
        void Add_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class CLS>
        void Add_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class CLS>
        void Add_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            m_allDels.push_back(temp);
        }
#endif

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
        const std::vector<DelegateSingle_Type>& GetArray()const noexcept
        {
            return this->m_allDels;
        }
        _declspec(property(get = getsize)) const size_t size;
        const size_t getsize()const noexcept
        {
            return m_allDels.size();
        }
        void swap(Delegate_base& _right)noexcept
        {
            this->m_allDels.swap(_right.m_allDels);
        }
        auto begin()const noexcept
        {
            return m_allDels.begin();
        }
        auto end()const noexcept
        {
            return m_allDels.end();
        }

        //触发调用
        Ty_ret Invoke(const Ty_params&... params)const 
        #if mycodes_delegate_cpp17
            noexcept(std::is_void_v<Ty_ret>)
        #endif
        {
            for (size_t i = 0; i < m_allDels.size(); i++)
            {
                #if mycodes_delegate_cpp17
                IF_CONSTEXPR(!std::is_void_v<Ty_ret>)
                #endif
                    if (i == m_allDels.size() - 1)
                    {
                        return m_allDels[i].Invoke(params...);
                    }

                m_allDels[i].Invoke(params...);
            }

            #if mycodes_delegate_cpp17
            if constexpr (!std::is_void_v<Ty_ret>)
                throw bad_invoke();
            #else
                throw bad_invoke();
            #endif
        }
        Ty_ret operator()(const Ty_params&... params)const 
        #if mycodes_delegate_cpp17
            noexcept(std::is_void_v<Ty_ret>)
        #endif
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
        bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        //删除静态委托
        bool Sub(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            return subDelegate(temp, m_allDels);
        }
        bool Sub(const DelegateSingle_Type& del)noexcept
        {
            return subDelegate(del, m_allDels);
        }
        bool operator-=(const DelegateSingle_Type& del)noexcept
        {
            return subDelegate(del, m_allDels);
        }

#if !mycodes_delegate_cpp20
        template<class CLS>
        bool Sub_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Sub_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Sub_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Sub_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
#endif

        template<class CLS>
        bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(Ty_ret(*__fun)(Ty_params...))const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(const DelegateSingle_Type& del)const noexcept
        {
            return haveDelegate(del, m_allDels);
        }

#if !mycodes_delegate_cpp20
        template<class CLS>
        bool Have_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Have_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Have_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class CLS>
        bool Have_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)const noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
#endif

    protected:
        Delegate_base() = default;
        Delegate_base(const Delegate_base&) = default;
        Delegate_base(Delegate_base&&) = default;
        Delegate_base(size_t size)
        {
            m_allDels.reserve(size);
        }
        std::vector<DelegateSingle_Type> m_allDels;
    };

    template<class Ty_ret, class... Ty_params>
    class Delegate :public Delegate_base<DelegateSingle,Ty_ret, Ty_params...>
    {
    public:
        Delegate(size_t size = 4) :Delegate_base<DelegateSingle, Ty_ret, Ty_params...>(size) {}
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
            return Delegate_base<DelegateSingle,Ty_ret, Ty_params...>::TryInvoke(params...);
        }
    };

    template<class Ty_ret, class...Ty_params>
    class Delegate<const Ty_ret&, Ty_params...> :public Delegate_base<DelegateSingle,const Ty_ret&, Ty_params...>
    {
    public:
        Delegate(size_t size = 4) :Delegate_base<DelegateSingle, const Ty_ret&, Ty_params...>(size) {}
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
            return Delegate_base<DelegateSingle, const Ty_ret&, Ty_params...>::TryInvoke(params...);
        }
    };

    template<class Ty_ret, class...Ty_params>
    class Delegate<const Ty_ret, Ty_params...> :public Delegate_base<DelegateSingle,const Ty_ret, Ty_params...>
    {
    public:
        Delegate(size_t size = 4) :Delegate_base<DelegateSingle, const Ty_ret, Ty_params...>(size) {}
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
            return Delegate_base<DelegateSingle, const Ty_ret, Ty_params...>::TryInvoke(params...);
        }
    };

    template<class...Ty_params>
    class Delegate<void, Ty_params...> :public Delegate_base<DelegateSingle, void, Ty_params...>
    {
    public:
        Delegate(size_t size = 4) :Delegate_base<DelegateSingle, void, Ty_params...>(size) {}
    };
}

namespace MyCodes
{
    /*  返回值为 void 时，特化的单委托类型。作为返回值为 Delegate_anyRet 类型的多播委托中的存储元素
      这种特化版本的单委托可以存储任意返回值的方法，但返回值会被舍弃。
      原理是该委托中存储了两个一般的单委托，分为顶层委托和底层委托两部分，底层委托以二进制方
    式存储实际指向的对象和方法，顶层委托则指向底层委托的 TryInvoke 方法。调用这种特化的委托
    时，由顶层委托调用底层委托，底层委托调用完毕后，返回值会被抛弃，然后顶层委托调用结束，返
    回 void。*/
    template<class _Ty_ret,class...Ty_params>
    class DelegateSingle_any
    {
        static_assert(std::is_void_v<_Ty_ret>, "DelegateSingle_any模板的第一个参数应为void");
    protected:
        using Byte = unsigned char;
        static CONSTEXPR size_t del_size = sizeof(DelegateSingle<void>);
        DelegateSingle<bool,const Ty_params&...> top_del;
        Byte bottom_del[del_size]{0};
    public:
        DelegateSingle_any() = default;
        DelegateSingle_any(const DelegateSingle_any& right)
        {
            this->operator=(right);
        }
        template<class CLS, class Ty_ret>
        DelegateSingle_any(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            this->Bind(__this, __fun);
        }
        template<class CLS, class Ty_ret>
        DelegateSingle_any(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            this->Bind(__this, __fun);
        }
        template<class Ty_ret>
        DelegateSingle_any(Ty_ret(* __fun)(Ty_params...))noexcept
        {
            this->Bind(__fun);
        }
        #if mycodes_delegate_cpp20
        template<class Lambda>
            requires is_lambda_any<Lambda, Ty_params...>
        #else
        template<class Lambda>
        #endif
        DelegateSingle_any(const Lambda& lam)
        {
            this->Bind(lam);
        }

        template<class CLS, class Ty_ret>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr = 
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class CLS, class Ty_ret>
        void Bind(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret>
        void Bind(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind(__fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        #if mycodes_delegate_cpp20
        template<class Lambda>
        requires is_lambda_any<Lambda,Ty_params...>
        #else
        template<class Lambda>
        #endif
        void Bind(const Lambda& lam)
        {
#if mycodes_delegate_cpp17
            using DelegateSingle_t = decltype(DelegateSingle(lam,&Lambda::operator() ));
#else
            using DelegateSingle_t = DelegateSingle<void, Ty_params...>;
#endif
            DelegateSingle_t* bottom_del_ptr =
                reinterpret_cast<DelegateSingle_t*>(bottom_del);
            bottom_del_ptr->Bind(lam);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle_t::TryInvoke);
        }

#if !mycodes_delegate_cpp17
        //绑定的对象属于有vbptr的类时，需要用 Bind_vbptr 函数绑定
        template<class Ty_ret, class CLS>
        void Bind_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind_vbptr(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret, class CLS>
        void Bind_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind_vbptr(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        //绑定的对象属于多继承的类时，需要用 Bind_multiple 函数绑定
        template<class Ty_ret, class CLS>
        void Bind_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind_multiple(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret, class CLS>
        void Bind_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle<Ty_ret, Ty_params...>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<Ty_ret, Ty_params...>*>(bottom_del);
            bottom_del_ptr->Bind_multiple(__this, __fun);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle<Ty_ret, Ty_params...>::TryInvoke);
        }
        template<class Ty_ret,class Lambda>
        void Bind_lambda(const Lambda& lam)
        {
            using DelegateSingle_t = DelegateSingle<Ty_ret, Ty_params...>;
            DelegateSingle_t* bottom_del_ptr =
                reinterpret_cast<DelegateSingle_t*>(bottom_del);
            bottom_del_ptr->Bind(lam);
            top_del.Bind(*bottom_del_ptr, &DelegateSingle_t::TryInvoke);
        }
#endif

        void UnBind()
        {
            DelegateSingle<void>* bottom_del_ptr =
                reinterpret_cast<DelegateSingle<void>*>(bottom_del);
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

        bool operator==(const DelegateSingle_any& right)const noexcept
        {
            const DelegateSingle<void>* __this =
                reinterpret_cast<const DelegateSingle<void>*>(this->bottom_del);
            const DelegateSingle<void>* __right =
                reinterpret_cast<const DelegateSingle<void>*>(right.bottom_del);
            return *__this == *__right;
        }
        const DelegateSingle_any& operator=(const DelegateSingle_any& right)noexcept
        {
            if (right.IsNull())
            {
                this->UnBind();
                return *this;
            }

            DelegateSingle<void>* __this =
                reinterpret_cast<DelegateSingle<void>*>(this->bottom_del);
            const DelegateSingle<void>* __right =
                reinterpret_cast<const DelegateSingle<void>*>(right.bottom_del);

            *__this = *__right;
            this->top_del = right.top_del;
            this->top_del._this.value =this->bottom_del;
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
    class Delegate_anyRet:public Delegate_base<DelegateSingle_any,void,Ty_params...>
    {
    public:
        using DelegateSingle_Type=typename Delegate_base<DelegateSingle_any, void, Ty_params...>::DelegateSingle_Type;
    public:
        Delegate_anyRet(size_t size = 4) :Delegate_base<DelegateSingle_any, void, Ty_params...>(size) {}
  
        template<class CLS, class Ty_ret>
        void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            this->m_allDels.push_back(temp);
        }
        template<class CLS, class Ty_ret>
        void Add(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            this->m_allDels.push_back(temp);
        }
        template<class Ty_ret>
        void Add(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            this->m_allDels.push_back(temp);
        }
        template<class Lambda>
        #if mycodes_delegate_cpp20
            requires is_lambda_any<Lambda,Ty_params...>
        #endif
        void Add(const Lambda& lam)
        {
            DelegateSingle_Type temp;
            temp.Bind(lam);
            this->m_allDels.push_back(temp);
        }

#if !mycodes_delegate_cpp20
        template<class CLS, class Ty_ret>
        void Add_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            this->m_allDels.push_back(temp);
        }
        template<class CLS,class Ty_ret>
        void Add_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            this->m_allDels.push_back(temp);
        }
        template<class CLS, class Ty_ret>
        void Add_multiple (const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            this->m_allDels.push_back(temp);
        }
        template<class CLS, class Ty_ret>
        void Add_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            this->m_allDels.push_back(temp);
        }
#endif  
        template<class CLS, class Ty_ret>
        bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Sub(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class Ty_ret>
        bool Sub(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class Lambda>
        #if mycodes_delegate_cpp20
            requires is_lambda_any<Lambda, Ty_params...>
        #endif
        void Sub(const Lambda& lam)
        {
            DelegateSingle_Type temp;
            temp.Bind(lam);
            return subDelegate(temp, this->m_allDels);
        }

#if !mycodes_delegate_cpp20
        template<class CLS, class Ty_ret>
        bool Sub_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Sub_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Sub_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Sub_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return subDelegate(temp, this->m_allDels);
        }
#endif

        template<class CLS, class Ty_ret>
        bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Have(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class Ty_ret>
        bool Have(Ty_ret(*__fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind(__fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class Lambda>
        #if mycodes_delegate_cpp20
            requires is_lambda_any<Lambda, Ty_params...>
        #endif
        void Have(const Lambda& lam)
        {
            DelegateSingle_Type temp;
            temp.Bind(lam);
            return haveDelegate(temp, this->m_allDels);
        }

#if !mycodes_delegate_cpp20
        template<class CLS, class Ty_ret>
        bool Have_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Have_vbptr(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_vbptr(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Have_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...))noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
        template<class CLS, class Ty_ret>
        bool Have_multiple(const CLS& __this, Ty_ret(CLS::* __fun)(Ty_params...)const)noexcept
        {
            DelegateSingle_Type temp;
            temp.Bind_multiple(__this, __fun);
            return haveDelegate(temp, this->m_allDels);
        }
#endif    
};
}

namespace MyCodes
{
    template<class DelegateType>
    class Delegate_view_base //委托视图基类
    {
        using DelegateSingle = typename DelegateType::DelegateSingle_Type;
    public:
        Delegate_view_base(DelegateType& del)
        {
            m_del = &del;
        }

        void Add(const DelegateSingle& del)noexcept
        {
            m_del->Add(del);
        }
        Delegate_view_base& operator+=(const DelegateSingle& del)noexcept
        {
            m_del->operator+=(del);
            return *this;
        }

        bool Sub(const DelegateSingle& del)noexcept
        {
            return m_del->Sub(del);
        }
        Delegate_view_base& operator-=(const DelegateSingle& del)noexcept
        {
            m_del->operator-=(del);
            return *this;
        }

        bool Have(const DelegateSingle& del)const noexcept
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
        void operator=(const Delegate_view_base&) = delete;

        DelegateType* m_del;
    };

    template<class Ty_ret,class...Ty_params>
    class Delegate_view :public Delegate_view_base<Delegate<Ty_ret, Ty_params...>>
    {
    public:
        Delegate_view(Delegate<Ty_ret, Ty_params...>& del)
            :Delegate_view_base<Delegate<Ty_ret, Ty_params...>>(del)
        {

        }
    };

    template<class... Ty_params>
    class Delegate_anyRet_view :public Delegate_view_base<Delegate_anyRet<Ty_params...>>
    {
    public:
        Delegate_anyRet_view(Delegate_anyRet<Ty_params...>& del)
            :Delegate_view_base<Delegate_anyRet<Ty_params...>>(del)
        {

        }
    };

    template<class Ty_ret, class...Ty_params>
    using Event = Delegate<Ty_ret, Ty_params...>;
    template<class Ty_ret, class...Ty_params>
    using Event_view = Delegate_view<Ty_ret, Ty_params...>;
}

#undef mycodes_delegate_cpp20
#undef mycodes_delegate_cpp17
#pragma pop_macro("IF_CONSTEXPR")
#pragma pop_macro("CONSTEXPR")
#pragma warning(default:6011)
#pragma warning(default:6101)

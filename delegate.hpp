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
        3、Empty 类仅作为强制类型转换时的中间类型，无实际意义，请不要使用。
        4、若意外调用了空委托，则会抛出 std::exception 类型的异常。
*/
#pragma once
#include<vector>
#include<exception>
#pragma warning(disable:6011)	
#pragma warning(disable:6101)   //让编译器不要发出空指针警告和未初始化_Out_参数警告

namespace MyCodes
{
    const std::exception nullexc("error:试图调用空委托");

    template<class DelType>
    inline bool subDelegate(const DelType& del,std::vector<DelType>& allDels)
    {//使用反向迭代器,把最后面的一个满足条件的委托移除
        auto findit = allDels.rend();
        for (auto it = allDels.rbegin(); it != allDels.rend(); it++)
        {
            if (*it == del)
            {
                findit = it;
                break;
            }
        }
        if (findit != allDels.rend())
        {
            allDels.erase(--(findit.base()));
            return true;
        }
        else
        {
            return false;
        }
    }

    template<class DelType>
    inline bool haveDelegate(const DelType& del, const std::vector<DelType>& allDels)
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

    template<class Tout, class... Tin>
    class Delegate_Single	//单委托
    {
    public:
        Delegate_Single() = default;
        Delegate_Single(const Delegate_Single&) = default;
        template<class DelPtr>
        Delegate_Single(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            this->Bind(__this, __fun);
        }
        template<class DelPtr>
        Delegate_Single(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...)const)
        {
            this->Bind(__this, __fun);
        }
        Delegate_Single(Tout(*__fun)(const Tin...))
        {
            this->Bind(__fun);
        }

        //绑定类对象和类成员函数
        template<class DelPtr>
        void Bind(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<DelPtr*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        template<class DelPtr>
        void Bind(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...)const)
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<DelPtr*>(&__this));
            _fun.this_fun = reinterpret_cast<decltype(_fun.this_fun)> (__fun);
        }
        //绑定静态函数
        void Bind(Tout(*__fun)(const Tin...))
        {
            _this = nullptr;
            _fun.static_fun = __fun;
        }

        bool IsNull()const
        {
            return _fun.isNull();
        }

        //触发调用
        Tout Invoke(const Tin&... tin)const noexcept(is_void_v<Tout>)
        {
            if (IsNull())
            {
                if constexpr (is_void_v<Tout>)
                    return;
                else
                    throw nullexc;
            }


            if (_this)
                return (_this->*(_fun.this_fun))(tin...);
            else
                return _fun.static_fun(tin...);
        }
        Tout operator()(const Tin&... tin)const noexcept(is_void_v<Tout>)
        {
            return Invoke(tin...);
        }

        //解除绑定
        void UnBind()
        {
            _this = nullptr;
            _fun.value = nullptr;
        }

        bool operator==(const Delegate_Single& right)const
        {
            return ((this->_fun.value == right._fun.value) &&
                this->_this == right._this);
        }
        const Delegate_Single& operator=(const Delegate_Single& right)
        {
            this->_this = right._this;
            this->_fun = right._fun;
            return *this;
        }

    protected:
        union _CallFun
        {
            void* value = nullptr;
            Tout(Empty::* this_fun)(const Tin...);
            Tout(*static_fun)(const Tin...);
            __forceinline bool isNull()const
            {
                return this->value == nullptr;
            }
        };

        Empty* _this = nullptr;
        _CallFun _fun;
    };

    template<class Tout, class... Tin>
    class Delegate_base //委托基类
    {
    public:
        //添加委托
        template<class DelPtr>
        void Add(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        template<class DelPtr>
        void Add(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...)const)
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }
        //添加静态委托
        void Add(Tout(*__fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__fun);
            m_allDels.push_back(temp);
        }
        void Add(const Delegate_Single<Tout, Tin...>& del)
        {
            if(!del.IsNull())
                this->m_allDels.push_back(del);
        }
        Delegate_base& operator+=(const Delegate_Single<Tout, Tin...>& del)
        {
            if (!del.IsNull())
                this->m_allDels.push_back(del);
            return *this;
        }

        void Clear()
        {
            m_allDels.clear();
        }
        bool IsNull()const
        {
            return m_allDels.size() == 0;
        }
        _declspec(property(get = getsize)) const size_t size;
        const size_t getsize()const
        {
            return m_allDels.size();
        }

        //触发调用
        Tout Invoke(const Tin&... tin)const noexcept(is_void_v<Tout>)
        {
            for (size_t i = 0; i < m_allDels.size(); i++)
            {
                if constexpr(!is_void_v<Tout>)
                    if (i == m_allDels.size() - 1)
                    {
                        return m_allDels[i].Invoke(tin...);
                    }

                m_allDels[i].Invoke(tin...);
            }

            if constexpr(!is_void_v<Tout>)
                throw nullexc;
        }
        Tout operator()(const Tin&... tin)const noexcept(is_void_v<Tout>)
        {
            return Invoke(tin...);
        }
        bool TryInvoke(const Tin&... tin)const
        {
            if (IsNull())
            {
                return false;
            }
            else
            {
                Invoke(tin...);
                return true;
            }
        }

        //删除委托
        template<class DelPtr>
        bool Sub(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        template<class DelPtr>
        bool Sub(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...)const)
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }
        //删除静态委托
        bool Sub(Tout(*__fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__fun);
            return subDelegate(temp, m_allDels);
        }
        bool Sub(const Delegate_Single<Tout, Tin...>& del)
        {
            return subDelegate(del, m_allDels);
        }
        bool operator-=(const Delegate_Single<Tout, Tin...>& del)
        {
            return subDelegate(del, m_allDels);
        }

        template<class DelPtr>
        bool Have(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...))const
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        template<class DelPtr>
        bool Have(const DelPtr& __this, Tout(DelPtr::* __fun)(const Tin...)const)const
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(Tout(*__fun)(const Tin...))const
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__fun);
            return haveDelegate(temp, m_allDels);
        }
        bool Have(const Delegate_Single<Tout, Tin...>& del)const
        {
            return haveDelegate(del, m_allDels);
        }

    protected:
        Delegate_base() = default;
        Delegate_base(const Delegate_base&) = default;
        Delegate_base(Delegate_base&&) = default;
        std::vector<Delegate_Single<Tout, Tin...>> m_allDels;
    };

    template<class Tout,class... Tin>
    class Delegate :public Delegate_base<Tout, Tin...>
    {
    public:
        bool TryInvoke(_Out_ Tout& out,_In_ const Tin&... tin)const
        {
            if (this->IsNull())
            {
                return false;
            }
            else
            {
                out = this->Invoke(tin...);
                return true;
            }
        }
        bool TryInvoke(const Tin&... tin)const
        {
            return Delegate_base<Tout, Tin...>::TryInvoke(tin...);
        }
    };

    //返回值为常量引用类型时的偏特化
    template<class Tout,class...Tin>
    class Delegate<const Tout&, Tin...> :public Delegate_base<const Tout&, Tin...>
    {
    public:
        bool TryInvoke(_Out_ Tout& out, _In_ const Tin&... tin)const
        {
            if (this->IsNull())
            {
                return false;
            }
            else
            {
                out = this->Invoke(tin...);
                return true;
            }
        }
        bool TryInvoke(const Tin&... tin)const
        {
            return Delegate_base<Tout, Tin...>::TryInvoke(tin...);
        }
    };

    template<class Tout, class...Tin>
    class Delegate<const Tout, Tin...> :public Delegate_base<const Tout, Tin...>
    {
    public:
        bool TryInvoke(_Out_ Tout& out, _In_ const Tin&... tin)const
        {
            if (this->IsNull())
            {
                return false;
            }
            else
            {
                out = this->Invoke(tin...);
                return true;
            }
        }
        bool TryInvoke(const Tin&... tin)const
        {
            return Delegate_base<Tout, Tin...>::TryInvoke(tin...);
        }
    };

    template<class...Tin>
    class Delegate<void, Tin...> :public Delegate_base<void, Tin...>
    {

    };
}

#pragma warning(default:6011)
#pragma warning(default:6101)

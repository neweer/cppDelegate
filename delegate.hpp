/*
    c++委托库
    用法示例:
        Delegate<void,const int&> del;          //声明一个委托
        std::vector<int> v;                     
        del.Add(&v, &decltype(v)::push_back);   //添加变量 v 的 push_back 方法
        del.Invoke(1);                          //调用委托，传入参数 1
                                                // v 中成功添加整数 1

    其他:
        1、为防止调用空委托，可以使用 TryInvoke 方法而不是 Invoke 方法。
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
#pragma warning(disable:6011,disable:6101)	//让编译器不要发出空指针警告和未初始化_Out_参数警告

namespace MyCodes
{
    const std::exception nullexc("error:试图调用空委托");
    using CallType = int;					//单个委托的调用方式
    constexpr CallType THIS_CALL = 1;		//类成员函数调用方式
    constexpr CallType STATIC_CALL = 2;		//静态函数调用方式

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
    template<class Tout, class... Tin>
    class Empty	//空类型
    {
    public:
        Tout InvokeFun(Tin...) {}
    };

    template<class Tout, class... Tin>
    class Delegate_Single	//单委托
    {
        using Empty = Empty<Tout, Tin...>;
    public:

        template<class DelPtr>
        //绑定类对象和类成员函数
        void Bind(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<DelPtr*>(__this));
            _fun = reinterpret_cast<decltype(_fun)> (__fun);
            calltype = THIS_CALL;
        }

        template<class DelPtr>
        void Bind(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)
        {
            _this = reinterpret_cast<decltype(_this)>(const_cast<DelPtr*>(__this));
            _fun = reinterpret_cast<decltype(_fun)> (__fun);
            calltype = THIS_CALL;
        }

        //绑定静态函数
        void Bind(Tout(*__fun)(const Tin...))
        {
            static_fun = __fun;
            calltype = STATIC_CALL;
        }

        bool IsNull()const
        {
            if ((_this == nullptr || _fun == nullptr) &&
                static_fun == nullptr)
                return true;
            else
                return false;
        }

        //触发调用
        Tout Invoke(const Tin&... tin)const noexcept(false)
        {
            if (IsNull())
                throw nullexc;

            if (calltype == THIS_CALL)
                return (_this->*_fun)(tin...);
            else
                return static_fun(tin...);
        }

        //解除绑定
        void Delete()
        {
            _this = nullptr;
            _fun = nullptr;
            calltype = 0;
            static_fun = nullptr;
        }

        bool operator==(const Delegate_Single& right)const
        {
            if (this->calltype != right.calltype)
                return false;

            if (this->calltype == THIS_CALL)
                return this->_this == right._this &&
                this->_fun == right._fun;
            else
                return this->static_fun == right.static_fun;
        }
    private:

        CallType calltype = 0;
        Empty* _this                    = nullptr;
        Tout(Empty::* _fun)(const Tin...) = nullptr;
        Tout(*static_fun)(const Tin...)   = nullptr;
    };

    template<class Tout, class... Tin>
    class Delegate
    {
    public:

        template<class DelPtr>
        //添加委托
        void Add(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }

        template<class DelPtr>
        void Add(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)
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
        Tout Invoke(const Tin&... tin)const noexcept(false)
        {
            for (size_t i = 0; i < m_allDels.size(); i++)
            {
                if (i == m_allDels.size() - 1)
                {
                    return m_allDels[i].Invoke(tin...);
                }
                m_allDels[i].Invoke(tin...);
            }

            throw nullexc;
        }

        bool TryInvoke(_Out_ Tout& out,const Tin&... tin)const
        {
            if (IsNull())
            {
                return false;
            }
            else
            {
                out=Invoke(tin...);
                return true;
            }
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

        template<class DelPtr>
        //删除委托
        bool Sub(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }

        template<class DelPtr>
        bool Sub(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)
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

        template<class DelPtr>
        bool Have(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))const
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }

        template<class DelPtr>
        bool Have(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)const
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

    private:

        std::vector<Delegate_Single<Tout, Tin...>> m_allDels;
    };

    template< class... Tin>
    class Delegate<void,Tin...>	//单参数委托
    {
    public:
        using Tout = void;
        template<class DelPtr>
        //添加委托
        void Add(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            m_allDels.push_back(temp);
        }

        template<class DelPtr>
        void Add(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)
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
        Tout Invoke(const Tin&... tin)const noexcept(false)
        {
            for (size_t i = 0; i < m_allDels.size(); i++)
            {
                if (i == m_allDels.size() - 1)
                {
                    return m_allDels[i].Invoke(tin...);
                }
                m_allDels[i].Invoke(tin...);
            }

            throw nullexc;
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

        template<class DelPtr>
        //删除委托
        bool Sub(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return subDelegate(temp, m_allDels);
        }

        template<class DelPtr>
        bool Sub(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)
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

        template<class DelPtr>
        bool Have(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...))const
        {
            Delegate_Single<Tout, Tin...> temp;
            temp.Bind(__this, __fun);
            return haveDelegate(temp, m_allDels);
        }

        template<class DelPtr>
        bool Have(DelPtr const* __this, Tout(DelPtr::* __fun)(const Tin...)const)const
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

    private:

        std::vector<Delegate_Single<Tout, Tin...>> m_allDels;
    };
}

#pragma warning(default:6011,default:6101)
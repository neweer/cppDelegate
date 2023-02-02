/*
    c++ί�п�
    �÷�ʾ��:
        Delegate<void,const int&> del;          //����һ��ί��
        std::vector<int> v;                     
        del.Add(&v, &decltype(v)::push_back);   //��ӱ��� v �� push_back ����
        del.Invoke(1);                          //����ί�У�������� 1
                                                // v �гɹ�������� 1

    ����:
        1��Ϊ��ֹ���ÿ�ί�У�����ʹ�� TryInvoke ���������� Invoke ������
           TryInvoke ������װ�˶�ί���Ƿ�Ϊ�յ��жϣ�ͨ��������ֵȷ���Ƿ��
           �óɹ������Ƿ�Ϊ�գ�������Ҫί�еķ���ֵ����Ӧ��ʹ�� TryInvoke 
           �����ذ汾���ڵ�һ������ǰ����⴫��Ҫ���ܷ���ֵ�ı�����
        2��������Ҫ�ಥί�У������ֱ��ʹ�� Delegate_Single �࣬������ֻ֧��
           �洢һ��ί�С�
        3��Empty �����Ϊǿ������ת��ʱ���м����ͣ���ʵ�����壬�벻Ҫʹ�á�
        4������������˿�ί�У�����׳� std::exception ���͵��쳣��
*/
#pragma once
#include<vector>
#include<exception>
#pragma warning(disable:6011,disable:6101)	//�ñ�������Ҫ������ָ�뾯���δ��ʼ��_Out_��������

namespace MyCodes
{
    const std::exception nullexc("error:��ͼ���ÿ�ί��");
    using CallType = int;					//����ί�еĵ��÷�ʽ
    constexpr CallType THIS_CALL = 1;		//���Ա�������÷�ʽ
    constexpr CallType STATIC_CALL = 2;		//��̬�������÷�ʽ

    template<class DelType>
    inline bool subDelegate(const DelType& del,std::vector<DelType>& allDels)
    {//ʹ�÷��������,��������һ������������ί���Ƴ�
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
    class Empty	//������
    {
    public:
        Tout InvokeFun(Tin...) {}
    };

    template<class Tout, class... Tin>
    class Delegate_Single	//��ί��
    {
        using Empty = Empty<Tout, Tin...>;
    public:

        template<class DelPtr>
        //�����������Ա����
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

        //�󶨾�̬����
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

        //��������
        Tout Invoke(const Tin&... tin)const noexcept(false)
        {
            if (IsNull())
                throw nullexc;

            if (calltype == THIS_CALL)
                return (_this->*_fun)(tin...);
            else
                return static_fun(tin...);
        }

        //�����
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
        //���ί��
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

        //��Ӿ�̬ί��
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

        //��������
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
        //ɾ��ί��
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

        //ɾ����̬ί��
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
    class Delegate<void,Tin...>	//������ί��
    {
    public:
        using Tout = void;
        template<class DelPtr>
        //���ί��
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

        //��Ӿ�̬ί��
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

        //��������
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
        //ɾ��ί��
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

        //ɾ����̬ί��
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
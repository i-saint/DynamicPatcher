#ifndef __Test_h__
#define __Test_h__


class DOL_Fixate IHoge
{
public:
    virtual ~IHoge() {}
    virtual void DoSomething()=0;
};

class Hoge : public IHoge
{
public:
    virtual void DoSomething();
    DOL_DeclareMemberFunction(int, MemFnTest, (int));
};

#endif // __Test_h__

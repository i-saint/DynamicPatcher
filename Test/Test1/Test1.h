// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#ifndef Test1_h
#define Test1_h


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

#endif // Test1_h

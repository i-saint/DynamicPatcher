// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>
#include <string>
#include "../../DynamicObjLoader.h"

class Hoge
{
public:
    Hoge() { printf("Hoge::Hoge()\n"); }
    ~Hoge() { printf("Hoge::~Hoge()\n"); }
    void ExecHoge() { printf("Hoge::ExecHoge()\n"); }
};

class Hage
{
public:
    Hage() { printf("Hage::Hage()\n"); }
    ~Hage() { printf("Hage::~Hage()\n"); }
    void ExecHage() { printf("Hage::ExecHage()\n"); }
};


void* Create(const std::string &name)
{
    volatile void *obj = NULL;
    std::string source = "obj = new "+name+"();";
    DOL_Eval(source.c_str());
    return (void*)obj;
}

void Delete(void *_obj, const std::string &name)
{
    volatile void *obj = _obj;
    std::string source = "delete reinterpret_cast<"+name+"*>(obj);";
    DOL_Eval(source.c_str());
}

void CallMemberFunction(void *_obj, const std::string &type, const std::string &func)
{
    volatile void *obj = _obj;
    std::string source = "reinterpret_cast<"+type+"*>(obj)->"+func+"();";
    DOL_Eval(source.c_str());
}

void TestReflection()
{
    {
        void *obj = Create("Hoge");
        CallMemberFunction(obj, "Hoge", "ExecHoge");
        Delete(obj, "Hoge");
    }
    {
        void *obj = Create("Hage");
        CallMemberFunction(obj, "Hage", "ExecHage");
        Delete(obj, "Hage");
    }
}


void TestLocalVariable()
{
    volatile int hoge = 10; // volatile: 最適化で消えるの防止
    volatile double hage = 1.0;
    DOL_Eval("printf(\"%d %lf\\n\", hoge, hage);");
    DOL_Eval("hoge+=10;");
    DOL_Eval("hage*=2.0;");
    DOL_Eval("printf(\"%d %lf\\n\", hoge, hage);");

    void *obj = NULL;
    DOL_Eval("obj = new Hoge();");

    printf("%d %lf\n", hoge, hage);
}

int main(int argc, char* argv[])
{
    DOL_EvalSetCompileOption("/EHsc");

    TestLocalVariable();
    TestReflection();

    return 0;
}

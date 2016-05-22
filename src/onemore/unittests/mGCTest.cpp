#include "../mGC.h"
#include "../mTable.h"
#include "../mFunction.h"
#include "../mString.h"
#include "../mValue.h"

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif // _MSC_VER

#include <stdlib.h>
#include <time.h>
#include <deque>
#include <string>

oms::GC g_gc(oms::GC::DefaultDeleter(), true);
std::deque<oms::Table *> g_globalTable;
std::deque<oms::Function *> g_globalFunction;
std::deque<oms::Closure *> g_globalClosure;
std::deque<oms::String *> g_globalString;

std::deque<oms::Table *> g_scopeTable;
std::deque<oms::Closure *> g_scopeClosure;
std::deque<oms::String *> g_scopeString;

template<typename T>
inline T RandomRange(T min, T max)
{
    return rand() % (max + 1 - min) + min;
}

template<typename T>
inline T RandomNum(T max)
{
    if (max == 0)
        return 0;
    else
        return RandomRange(static_cast<T>(0), max - 1);
}

void MinorRoot(oms::GCObjectVisitor *v)
{
    for (auto t : g_scopeTable)
        t->Accept(v);
    for (auto c : g_scopeClosure)
        c->Accept(v);
    for (auto s : g_scopeString)
        s->Accept(v);
}

void MajorRoot(oms::GCObjectVisitor *v)
{
    for (auto t : g_globalTable)
        t->Accept(v);
    for (auto f : g_globalFunction)
        f->Accept(v);
    for (auto c : g_globalClosure)
        c->Accept(v);
    for (auto s : g_globalString)
        s->Accept(v);
    MinorRoot(v);
}

oms::Table * RandomTable();
oms::Function * RandomFunction();
oms::Closure * RandomClosure();
oms::String * RandomString();
oms::Value RandomValue(bool exclude_table = false);

oms::Table * RandomTable()
{
    auto t = g_gc.NewTable();

    int array_count = RandomNum(10);
    for (int i = 0; i < array_count; ++i)
        t->SetArrayValue(i + 1, RandomValue(true));

    int hash_count = RandomNum(20);
    for (int i = 0; i < hash_count; ++i)
        t->SetValue(RandomValue(true), RandomValue(true));

    return t;
}

oms::Function * RandomFunction()
{
    auto f = g_gc.NewFunction();

    auto s = RandomString();
    f->SetModuleName(s);
    f->SetLine(RandomNum(1000));

    int instruction_count = RandomRange(10, 1000);
    for (int i = 0; i < instruction_count; ++i)
    {
        unsigned int op_min = oms::OpType_LoadNil;
        unsigned int op_max = oms::OpType_GetGlobal;
        oms::OpType op = static_cast<oms::OpType>(RandomRange(op_min, op_max));
        oms::Instruction instruction(op, RandomNum(128), RandomNum(128), RandomNum(128));
        f->AddInstruction(instruction, i);
    }

    int const_num = RandomNum(5);
    for (int i = 0; i < const_num; ++i)
        f->AddConstNumber(RandomNum(100000));

    int const_str = RandomNum(5);
    for (int i = 0; i < const_str; ++i)
        f->AddConstString(RandomString());

    CHECK_BARRIER(g_gc, f);

    return f;
}

oms::Closure * RandomClosure()
{
    if (g_globalFunction.empty())
        g_globalFunction.push_back(RandomFunction());

    auto index = RandomNum(g_globalFunction.size());
    auto proto = g_globalFunction[index];

    auto c = g_gc.NewClosure();
    c->SetPrototype(proto);
    return c;
}

oms::String * RandomString()
{
    std::string str;
    int count = RandomRange(1, 150);
    for (int i = 0; i < count; ++i)
        str.push_back(RandomRange('a', 'z'));

    auto s = g_gc.NewString();
    s->SetValue(str);
    return s;
}

oms::Value RandomValue(bool exclude_table)
{
    oms::ValueT type = oms::ValueT_Nil;
    int percent = RandomRange(1, 100);
    if (percent <= 20)
        type = oms::ValueT_Nil;
    else if (percent <= 30)
        type = oms::ValueT_Bool;
    else if (percent <= 60)
        type = oms::ValueT_Number;
    else if (percent <= 70)
        type = oms::ValueT_String;
    else if (percent <= 80)
        type = oms::ValueT_Closure;
    else if (percent <= 90)
        type = oms::ValueT_CFunction;
    else
        type = exclude_table ? oms::ValueT_Number : oms::ValueT_Table;

    oms::Value value;
    value.type_ = type;
    switch (type)
    {
        case oms::ValueT_Nil:
            break;
        case oms::ValueT_Bool:
            value.bvalue_ = RandomRange(0, 1) ? true : false;
            break;
        case oms::ValueT_Number:
            value.num_ = RandomNum(100000);
            break;
        case oms::ValueT_Obj:
            value.obj_ = RandomString();
            break;
        case oms::ValueT_String:
            value.str_ = RandomString();
            break;
        case oms::ValueT_Closure:
            value.closure_ = RandomClosure();
            break;
        case oms::ValueT_Table:
            value.table_ = RandomTable();
            break;
        case oms::ValueT_CFunction:
            break;
        default:
            break;
    }

    return value;
}

void NewObjInGlobal()
{
    int percent = RandomRange(1, 100);
    if (percent <= 20)
    {
        g_globalTable.push_back(g_gc.NewTable(oms::GCGen2));
    }
    else if (percent <= 50)
    {
        g_globalString.push_back(g_gc.NewString(oms::GCGen2));
    }
    else if (percent <= 60)
    {
        if (!g_globalFunction.empty())
        {
            auto index = RandomNum(g_globalFunction.size());
            auto proto = g_globalFunction[index];
            auto c = g_gc.NewClosure(oms::GCGen1);
            c->SetPrototype(proto);
            g_globalClosure.push_back(c);
        }
    }
}

void RunScope(int count)
{
    for (int i = 0; i < count; ++i)
    {
        int percent = RandomRange(1, 100);
        if (percent <= 15)
            RandomValue();
        else if (percent <= 20)
            g_globalFunction.push_back(RandomFunction());
        else if (percent <= 30)
            g_scopeString.push_back(RandomString());
        else if (percent <= 40)
            g_scopeClosure.push_back(RandomClosure());
        else if (percent <= 50)
            g_scopeTable.push_back(RandomTable());
        else if (percent <= 55)
            NewObjInGlobal();
    }
}

void TouchGlobalTable(int count)
{
    if (g_globalTable.empty())
        return ;

    std::size_t total_scope = 0;
    total_scope += g_scopeTable.size();
    total_scope += g_scopeString.size();
    total_scope += g_scopeClosure.size();
    if (total_scope == 0)
        return ;

    for (int i = 0; i < count; ++i)
    {
        auto setter = [&](oms::Value &v, std::size_t index) {
            if (index < g_scopeTable.size())
            {
                v.type_ = oms::ValueT_Table;
                v.table_ = g_scopeTable[index];
            }
            else if (index < g_scopeTable.size() + g_scopeString.size())
            {
                index -= g_scopeTable.size();
                v.type_ = oms::ValueT_String;
                v.str_ = g_scopeString[index];
            }
            else
            {
                index -= g_scopeTable.size() + g_scopeString.size();
                v.type_ = oms::ValueT_Closure;
                v.closure_ = g_scopeClosure[index];
            }
        };

        oms::Value key;
        oms::Value value;
        auto key_index = RandomNum(total_scope);
        auto value_index = RandomNum(total_scope);
        setter(key, key_index);
        setter(value, value_index);

        auto global_index = RandomNum(g_globalTable.size());
        auto global = g_globalTable[global_index];
        global->SetValue(key, value);
        CHECK_BARRIER(g_gc, global);
    }
}

void FreeGlobal(int count)
{
    for (int i = 0; i < count; ++i)
    {
        std::size_t size = 0;
        size += g_globalFunction.size();
        size += g_globalClosure.size();
        size += g_globalString.size();
        size += g_globalTable.size();

        if (size == 0)
            return ;

        auto index = RandomNum(size);
        if (index < g_globalFunction.size())
        {
            g_globalFunction.erase(g_globalFunction.begin() + index);
        }
        else if (index < g_globalFunction.size() + g_globalClosure.size())
        {
            index -= g_globalFunction.size();
            g_globalClosure.erase(g_globalClosure.begin() + index);
        }
        else if (index < g_globalFunction.size() + g_globalClosure.size() +
                 g_globalString.size())
        {
            index -= g_globalFunction.size() + g_globalClosure.size();
            g_globalString.erase(g_globalString.begin() + index);
        }
        else
        {
            index -= g_globalFunction.size() + g_globalClosure.size() + g_globalString.size();
            g_globalTable.erase(g_globalTable.begin() + index);
        }
    }
}

void RandomLoop()
{
    int free_global_count_max = 20;

    while (true)
    {
        g_scopeClosure.clear();
        g_scopeString.clear();
        g_scopeTable.clear();

        int scope_count = RandomRange(1, 1000);
        RunScope(scope_count);

        TouchGlobalTable(RandomNum(scope_count));

        int free_count = RandomRange(1, free_global_count_max++);
        FreeGlobal(free_count);

        if (free_global_count_max >= 1000)
            free_global_count_max = 20;

        g_gc.CheckGC();
#ifdef _MSC_VER
        Sleep(RandomRange(1, 20));
#else
        usleep(RandomRange(1000, 20000));
#endif // _MSC_VER
    }
}

//int main()
//{
//    srand(static_cast<unsigned int>(time(nullptr)));
//    g_gc.SetRootTraveller(MinorRoot, MajorRoot);
//    RandomLoop();
//    return 0;
//}

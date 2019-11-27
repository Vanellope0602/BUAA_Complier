//
//  Func_attribute.h
//  解耦合_代码生成
//
//  Created by 王珊珊 on 2019/11/15.
//  Copyright © 2019 vanellope. All rights reserved.
//
#include <map>
#include <set>
#include <vector>
#ifndef Func_attribute_h
#define Func_attribute_h
using namespace std;
class Func_attribute
{
   public:
    string Func_name;
    int func_sp_offset = 0; // 整个函数的常量，变量（包括参数）相对该函数的偏移，初始值为0
    int return_type; // 0 = void, 1 = int, 2 = char
    // 参数列表<para_name, para_type>, 要按顺序，所以用vector
    vector<pair<string, string>> para_table;
    // 下面的局部标识符 都可以和函数重名，但是互相之间不能重名
    // set 容器会自动去重， Map可以用哈希查找，变量和常量不需要研究顺序
    set<string> para_name; // 特别设置一个参数名，方便与常量和变量比较是否重定义

    // 常量列表<name, type>
    map<string, string> cons_table;
    // 变量列表, 可能是数组 <name, <type, size>>
    map<string, pair<string, int>> vars_table;
    
    // 关于函数局部常量和变量
    map<string, int> int_cons_value; // <cons_name, Int value>
    map<string, char> char_cons_value; // <cons_name, char Value>
    map<string, int> vars_addr; // <name , address>，这个也可以用于idenfr, 中间临时变量, ret 的地址分配
    // 参数对应的地址也视为普通变量使用， 这里面也包含参数名字，调用时配合 para table 使用
    // vars_addr最后一个位置 -4 这个地址应该存 total_offset这个函数的总体相对偏移值
    // 其变量名叫做：TOTAL_OFFSET_ADDR "total_offset"，分配 allocVarAddr(TOTAL_OFFSET_ADDR, 0)
    // 每次函数定义完毕之后都要往 func_sp_offset($sp) 这个地址里面存 func_sp_offset 的值
    
    Func_attribute();
    Func_attribute(string name, int type);
    int getType();
    // type 写的是sym (INTTK / CHARTK)
    void addPara(string type, string name);
    void addCons(string type, string name);
    void addVars(string type, string name, int arr_size);
    
    void allocVarAddr(string name, int size);
    int getFuncSpOffset();
};

#endif /* Func_attribute_h */


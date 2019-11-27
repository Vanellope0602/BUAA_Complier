//
//  Func_attribute.cpp
//  解耦合_代码生成
//
//  Created by 王珊珊 on 2019/11/19.
//  Copyright © 2019 vanellope. All rights reserved.
//

#include <iostream>
#include <map>
#include "Func_attribute.h"
#include "MyHeader.h"
using namespace std;
// 函数对应常量和变量名字 <func_name, 属性 >

Func_attribute::Func_attribute(){
    
}
Func_attribute::Func_attribute(string name, int type) {
    Func_name = name;
    return_type = type;
}
int Func_attribute::getType() {
    return return_type;
}
void Func_attribute::addPara(string type, string name) { // 参数列表要严格按照顺序
    if (para_name.count(name) == 0) { // 没有定义过 可以放
        para_table.push_back(make_pair(name,type));
        para_name.insert(name);
    } else {
        //cout << "Line : " << curLine << "重定义参数 :" << name << " in function " << Func_name<< endl;
        error(double_def_name);
    }
}
void Func_attribute::addCons(string type, string name) { // 常量不会有数组
    if (cons_table.count(name) == 0
        && vars_table.count(name) == 0
        && para_name.count(name) == 0) {
        cons_table.insert(make_pair(name, type)); // make_pair 无需写出pair类型
    } else {
//        cout << "Line : " << curLine << "重定义常量 :" << name << " in function "
//            << Func_name<< endl;
        error(double_def_name);
    }
}
void Func_attribute::addVars(string type, string name, int arr_size = 0) { // 默认size = 0 不是数组
    if (cons_table.count(name) == 0
        && vars_table.count(name) == 0
        && para_name.count(name) == 0) { // 变量也不能和参数重名
        vars_table.insert(make_pair(name, make_pair(type, arr_size))); // 注意把name放在前面！
    } else {
//        cout << "Line : " << curLine << "重定义变量 :" << name << " in function "
//            << Func_name << endl;
        error(double_def_name);
    }
}

void Func_attribute::allocVarAddr(string name, int size = 0) {
    if (size == 0 || size == 1) {
        func_sp_offset -= 4;
    } else {
        func_sp_offset = func_sp_offset - 4*size;
    }
    vars_addr.insert(make_pair(name, func_sp_offset));
}

int Func_attribute::getFuncSpOffset() {
    return func_sp_offset;  // func_sp_offset 是该函数实时的最大（最后一个变量）的相对偏移量
}

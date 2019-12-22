//
//  Mid_code_struct.h
//  代码生成
//
//  Created by 王珊珊 on 2019/11/14.
//  Copyright © 2019 vanellope. All rights reserved.
//
using namespace std;
#ifndef Mid_code_struct_h
#define Mid_code_struct_h

#define FOR "for"
#define DO "do"
#define WHILE "while"
#define IF "if"
#define FUNC "func"

#define arithmetic_assign 1
#define call_function 2
#define return_state 3
#define declare_vars 4
#define declare_cons 5  // 常量声明
#define assign_express 6 // 赋值语句表达式

class midCode { // 由于是一个四元式，所以这里要设计一个midCode类能通用与所有中间四元式
  public:
    string result;  // 算术表达式中，result 表示左侧计算结果，跳转指令中，result表示要跳转到的标签名字
    string arg1;    //
    string op;
    string arg2;
    int type;   //  种类代号，详情见README文档
    
    midCode();
    // type = 1 算数表达式，赋值, type = 4，变量常量声明， 参数列表
    // type = 5 条件, type = 6写语句,，共有4个参数，
    midCode(int type, string result, string arg1, string op, string arg2);
    
    //  type = 2 二元，函数声明头，进栈，数组赋值， 函数调用，返回语句，无条件跳转, type = 7, 读语句
    midCode(int type, string result, string arg1);
    
    // type = 3, 真假跳转
    midCode(int type, string result, string arg1, string op);
    
    // type = 8, 标签，只需要一个参数
    midCode(int type, string result);
    
    
};


void outputMid(midCode item);
pair<string, string> genLabel(string type);    // 标签是成对儿生成的
string genTempReg();

void printMid();
string getCurTempReg();
string getLastTempReg();
int midCodeSize();
midCode getNextMidCode(int i);
void modifyMidCode(midCode code, int pos);
#endif /* Mid_code_struct_h */

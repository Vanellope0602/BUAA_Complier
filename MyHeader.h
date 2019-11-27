//
//  MyHeader.h
//  解耦合_代码生成
//
//  Created by 王珊珊 on 2019/11/13.
//  Copyright © 2019 vanellope. All rights reserved.

#include <vector>
using namespace std;

#ifndef MyHeader_h
#define MyHeader_h

#define IDENFR_len 6    // 用substr于获取当前 idenfr
#define STRCON_len 6    // 用 substr 获取当前字符串
// 类别码字符串定义
#define IDENFR "IDENFR"
#define INTCON "INTCON"
#define CHARCON "CHARCON"
#define STRCON "STRCON"

#define CONSTTK "CONSTTK"
#define INTTK "INTTK"
#define CHARTK "CHARTK"
#define VOIDTK "VOIDTK"
#define MAINTK "MAINTK"
#define IFTK "IFTK"
#define ELSETK "ELSETK"
#define DOTK "DOTK"
#define WHILETK "WHILETK"
#define FORTK "FORTK"
#define SCANFTK "SCANFTK"
#define PRINTFTK "PRINTFTK"
#define RETURNTK "RETURNTK"
#define PLUS "PLUS"
#define MINU "MINU"
#define MULT "MULT"
#define DIV "DIV"

#define LSS "LSS" // <
#define LEQ "LEQ" // <=
#define GRE "GRE" // >
#define GEQ "GEQ" // >=
#define EQL "EQL" // ==
#define NEQ "NEQ" // !=
#define ASSIGN "ASSIGN" // =

#define SEMICN "SEMICN"
#define COMMA "COMMA"
#define LPARENT "LPARENT"
#define RPARENT "RPARENT"
#define LBRACK "LBRACK"
#define RBRACK "RBRACK"
#define LBRACE "LBRACE"
#define RBRACE "RBRACE"

// 文件路径定义
#define FILEPATH "/Users/wangshanshan/Desktop/编译技术/编译课设/编译上机/代码生成/解耦合_代码生成/解耦合_代码生成/testfile.txt"
#define WRITEPATH "/Users/wangshanshan/Desktop/编译技术/编译课设/编译上机/代码生成/解耦合_代码生成/解耦合_代码生成/error.txt"
#define MIPSPATH "/Users/wangshanshan/Desktop/编译技术/编译课设/编译上机/代码生成/解耦合_代码生成/解耦合_代码生成/mips.txt"

// 错误码定义
// 输出错误类别码方式：字母 'a' + macro
#define unvalid_char 0 // a 非法符号或不符合词法
#define double_def_name 1 // b 名字重定义
#define undefined_name 2  // c 名字未定义
#define para_num_wrong 3  // d 参数个数不匹配
#define para_type_wrong 4 // e 参数类型不匹配
#define condition_type_wrong 5 // f 条件判断中出现不合法类型
#define return_stat_occur 6  // g 无返回值的函数存在不匹配的return语句
#define missing_return 7 // h 有返回值的函数缺少return语句或存在不匹配的return语句
#define array_index_int 8 // i 数组元素的下标只能是整型表达式
#define cons_not_be_changed 9 // j 不能改变常量的值
#define should_have_semicn 10 // k 应为分号
#define should_have_rparent 11 // l 应为右小括号’)’
#define should_have_rbrack 12 // m 应为右中括号’]’
#define missing_while 13 // n do-while语句缺少 while
#define cons_only_int_or_char 14 // o 常量定义中=后面只能是整型或字符型常量

void clearBuf();    // 清空缓存的输出函数
bool can_follow_idenfr(char ch);
int isUnderline(char ch);   // 判断是否是下划线
string isRemain(string buff); // 判断是否是保留字
void error(int error_type);

int checkIdenfrDef(string name, string type, bool is_var, int size); // 查标识符是否重定义，并加入符号表中
// 递归下降函数
void program();
void cons_des();
void cons_def();
bool integer();
bool unsigned_int();
void var_des();
void var_def(string type);
void name_or_array(string type);
void return_func();
void return_func_begin_para();
void dec_head();
void para_table(string curFunc);
void non_return_func();
void mainProgram();
void compound_statement();
void statement_column();
bool is_Statement_head();
void statement();
void condition_stat();
void condition();
void cir_stat();
void step_length();
string call_stat();
void value_para_table(string real_name);
string express();
string item();
string factor();
void assign_stat();
void read_stat();
void write_stat();
void isString();
void return_stat();

string genStrName();
void genMips();

string getReg();

void two_elem_assign(string result, string arg1);
void load_argument(string arg, string reg);
#endif /* MyHeader_h */

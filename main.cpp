//
//  main.cpp
//  代码生成
//
//  Created by 王珊珊 on 2019/11/13.
//  Copyright © 2019 vanellope. All rights reserved.
//
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include "MyHeader.h" // 各种宏定义
#include "Func_attribute.h"
#include "Mid_code_struct.h"

using namespace std;
const int original_sp = 2147479548;
int sp_offset = 0; // $sp 地址，用于存各种变量，常量
int cur_real_sp = original_sp;
vector<string> push_vector; // 函数调用的时候临时存一下实参的名字
//stack<pair<string, int>> call_env_stack; // < 要跳回去的函数名，-(要加回去的函数总体偏移+4) >
map<string, string> strContainer; // 一个容器存下来所有的字符串，用于生成mips的时候 .data asciiz
int strNum = 0;
// 调用某个标识符变量/常量时，若在本函数内找到名字，则为局部的，再找全局的，否则未定义
// <constant_name, type>, <variant_name, <type, size>>
map<string, string> global_cons; // 全局常量名字 不能与函数重名
map<string, int> global_int_cons_value; // <cons_name, Int value>
map<string, char> global_char_cons_value; // <cons_name, char Value>
map<string, pair<string, int>> global_vars; // 全局变量名字，有可能是数组，size默认是0(非数组)
vector<string> global_vars_mips_data; // 全局变量放 mips .data标签

int sregNo = 0; // 需要保存的寄存器分配，s0-s7
int tregNo = 0; // 临时寄存器分配, t0 - t9
int saveRegisterGroup[8]; // s0-s7寄存器组，1 代表占用锁定，0代表可以用 getReg 分配一个过去
map<string, int> lockedRegMap; // 标志 <var_name, regNo> 该变量是否分配了一个s寄存器
// 由于s寄存器涉及解锁问题，所以存的是寄存器号，方便字符串拼接
int tmpRegisterGroup[10];   // t0-t9
map<string, string> concludeVarMap;    // <var_name, regName> （目前只用来存归纳变量,原则上不超过5个）
// 这个 t 寄存器自始至终都可以用，不用释放，直接存寄存器名字
ofstream outfile;
ifstream infile;
ofstream outmips;
string sym;     // 一个全局变量，用来标记当前最新读进来的类别码
string idenfr;  // 一个全局变量，用来记录当前最新的标识符
string cur_integer; // 一个全局变量，用来标记当前最新的带符号整数，是存到中间代码的字符串
string cur_str_con; // 一个全局变量，用来标记当前最新的STRCON
char cur_char_con = ' '; // 一个全局变量，用来标记当前最新的 CHARCON  单个字符常量，初始化为空格
bool is_defining_con_var = true; // 全局变量，初始为true，递归分析后为false，用来标记现在是否在变量/常量定义阶段
string curFunc = "NULL"; // 一个全局变量，用来标记当前在哪个函数里面，初始是全局所以为 NULL
int cur_exp_type = 0; // 用于标记当前表达式的类型，0 暂无，1 int, 2 char，每次<express> 末尾都要更新
int cur_int_con = 0; // 标记当前读进来的无符号整数
bool have_return_stat = false; // 用于标记当前局域是否出现了 return(***) 语句,"return;"不算，每次新函数定义时设置为 F
bool arr_be_assigned = false;   // 赋值语句是否是数组元素被赋值
string buffer[500000];
int buf_pointer = 0; // 用来标记缓冲区到了哪里
// 错误处理相关全局参数
int last_sym_line = 1; // 缺分号应该报在应该有分号的那一行
int curLine = 1;    // 标记当前行数, 初始化1

string getSym();    // 返回类别符的词法分析函数
// 添加有关函数属性的时候，直接调用它自己的函数就可以了，找到对应的函数内区域 全局function_name即可

map<string, Func_attribute> Func_attr;


int main() {
    infile.open(FILEPATH, ios::binary);
    outfile.open(WRITEPATH, ios::trunc); // 以写模式打开文件
    
    sym = getSym(); // 预读一个符号
    program();

    infile.close();
    outfile.close();
    
    printMid();
    genMips();
    
    return 0;
}



bool can_follow_idenfr(char ch) {
    if (ch == '{' || ch == '=' || ch == '!' || ch == '[' || ch == ']'
    || ch == '(' || ch == ')' || ch == ',' || ch == '<' || ch == '>'
    || ch == ';' || isspace(ch) || ch == '+' || ch == '-' || ch == '*' || ch == '/') {
        return true;
    } else {
        return false;
    }
}

int isUnderline(char ch) {
    if (ch == '_') {
        return 1;
    } else return 0;
}
// 判断是否为保留字的函数内容
string isRemain(string buff) {  // 会把类别码写到buffer里
    //cout << "Into is remain function: " << buff << endl;
    if (buff == "int") {
        buffer[buf_pointer++] = "INTTK int";
        return INTTK;
    } else if (buff == "char") {
        buffer[buf_pointer++] = "CHARTK char";
        return CHARTK;
    } else if (buff == "void") {
        buffer[buf_pointer++] = "VOIDTK void";
        return VOIDTK;
    } else if (buff == "const") {
        buffer[buf_pointer++] = "CONSTTK const";
        return CONSTTK;
    } else if (buff == "if") {
        buffer[buf_pointer++] = "IFTK if";
        return IFTK;
    } else if (buff == "else") {
        buffer[buf_pointer++] = "ELSETK else";
        return ELSETK;
    } else if (buff == "do") {
        buffer[buf_pointer++] = "DOTK do";
        return DOTK;
    } else if (buff == "while") {
        buffer[buf_pointer++] = "WHILETK while";
        return WHILETK;
    } else if (buff == "for") {
        buffer[buf_pointer++] = "FORTK for";
        return FORTK;
    } else if (buff == "scanf") {
        buffer[buf_pointer++] = "SCANFTK scanf";
        return SCANFTK;
    } else if (buff == "printf") {
        buffer[buf_pointer++] = "PRINTFTK printf";
        return PRINTFTK;
    } else if (buff == "return") {
        buffer[buf_pointer++] = "RETURNTK return";
        return RETURNTK;
    } else if (buff == "main") {
        buffer[buf_pointer++] = "MAINTK main";
        return MAINTK;
    } else {
        return "NULL"; // 空字符串说明什么都不是
    }
}
void error(int error_type) {
    // 需要用到的参数 curLine，错误类别码
    char error_code = 'a' + error_type;
    if (error_type == should_have_semicn) {
        cout << "报缺少分号的行号 " << last_sym_line << endl;
        outfile << last_sym_line << " " << error_code << endl;
    } else {
        outfile << curLine << " " << error_code << endl;
    }
}
int checkIdenfrDef(string type, string name, bool is_var, int size = 0) {
    // type 为该标识符号类型: INTTK / CHARTK 默认是变量，没有数组大小，size = 0
    // 传进来的是一个idenfr的标识符名字, is_define? 检查是否重定义然后插入
    // 判断是在哪个 局域函数块 里面
    // 若现在不是定义阶段，检查未定义，穿参数 type = "NULL", string name, true
    if (curFunc == "NULL") { //
        //cout << "check idenfr 现在是全局变量or常量" << endl;
        if (is_defining_con_var) { // 检查全局重定义，没有函数可调用 只能手写
            if (global_cons.count(name) == 1) {
                cout << "Line : " << curLine << " 全局常量重定义 " << name << endl;
                error(double_def_name);
            } else if (global_vars.count(name) == 1) {
                cout << "Line : " << curLine << " 全局变量重定义 " << name << endl;
                error(double_def_name);
            } else { // 没有重定义！那么现在来定义一下！
                if (is_var) { // 变量，还要看是不是数组 size == 0? (不用，size照样添加进去，使用的时候看是不是0就行)
                    global_vars.insert(make_pair(name, make_pair(type, size)));
                    // 给这个单个全局变量分配一个内存地址，注意 size == 0? 是不是数组
                    //数组名代表arr[0]在最底下，也就是 sp 在的地方
                    string global_data; // global_var1: .space 400
                    if (size == 0 || size == 1) {
                        global_data = name + ": .space 4";
                    } else {
                        global_data = name + ": .space " + to_string(size*4);
                    }
                    global_vars_mips_data.push_back(global_data);
                } else { // 常量定义，直接添加 insert <name, type>
                    global_cons.insert(make_pair(name, type));
                    // 全局变量存值，在外面的常量定义的时候进行
                }
            }
        }
    }
    else { // 在某函数内， 然后调用对应的函数的 func_attribute
        if (is_defining_con_var) { // 检查重定义
            if (is_var) { // 直接添加，在Func_attribute类里面已经写好了查重函数,但是无法与现有函数名查重
                if (Func_attr.count(name) == 1) {
                    printf("Line %d : 变量名与函数名重复\n", curLine);
                    error(double_def_name);
                } else {
                    Func_attr[curFunc].addVars(type, name, size);
                    // 给这个函数内单个变量分配一个内存地址，size > 0 则按照一整个数组划分，
                    // 数组名代表arr[0]在最底下，也就是 sp 在的地方
                    Func_attr[curFunc].allocVarAddr(name, size);
                }
            } else { // 函数内常量 , 在外面存值
                if (Func_attr.count(name) == 1) {
                    printf("Line %d : 常量名与函数名重复\n", curLine);
                    error(double_def_name);
                } else {
                    Func_attr[curFunc].addCons(type, name);
                }
            }
        } else { // 使用标识符ing, 检查是否未定义,有返回值类型
            if (Func_attr[curFunc].para_name.count(name) == 1) { //para_name无序，加快搜索速度
                for (int i = 0; i < Func_attr[curFunc].para_table.size(); i++) { // 遍历查找参数表
                    if (Func_attr[curFunc].para_table[i].first == name) { // 找参数类型,拿出para_table
                        return (Func_attr[curFunc].para_table[i].second == INTTK) ? 1:2;
                    }
                }
            } else if (Func_attr[curFunc].cons_table.count(name) == 1) {
                return (Func_attr[curFunc].cons_table[name] == INTTK)? 1:2;
            } else if (Func_attr[curFunc].vars_table.count(name) == 1) { // <name, <type,size>>
                return (Func_attr[curFunc].vars_table[name].first == INTTK)? 1:2;
            } else if (global_cons.count(name) == 1) { // 找全局常量有没有定义过
                return (global_cons[name] == INTTK)? 1:2;
            } else if (global_vars.count(name) == 1) {
                return (global_vars[name].first == INTTK)? 1:2;
            }
            else if (name[0] != '#'){ // 使用标识符查找未定义的时候也不用判断类型，所以也可以返回0
                cout << "Line : " << curLine << " 在函数 " << curFunc << " 中未定义 "<< name << " 此时type = " << type << is_var << endl;
                error(undefined_name); // 如果是 #开头的说明是中间临时变量，不用查有没有定义
            }
        }
    }
    return 0; // 不是检查未定义的，外层也不用取返回值
}

string getSym() {
    last_sym_line = curLine;
    char ch;
    string buff = "";    // 用来临时存储保留字
    while((ch = infile.get()) != EOF) {
        if (ch == '\n') {
            curLine++;
        }
        else if(isspace(ch)) {
            continue;
        }
        else if(isalpha(ch) || isUnderline(ch)) { // a-z A-Z _ 字母也包括下划线
            while(isalnum(ch) || isUnderline(ch) || !can_follow_idenfr(ch)){ // ab, 0 _
                if (!can_follow_idenfr(ch) && !(isalnum(ch) || isUnderline(ch))) {
                    printf("Line %d : 标识符/名字后面出现非法类型 %c \n", curLine, ch);
                    error(unvalid_char); // 然后继续
                }
                buff = buff + ch;
                ch = infile.get();
            } // ch 是其他字符了 retract
            infile.seekg(-1, ios::cur);
            string tmp = isRemain(buff);
            if (tmp == "NULL") { // 是标识符
                //cout << "Identify name is " << buff << endl;
                // 每次来要更新一下当前的 idenfr
                idenfr = buff;
                buffer[buf_pointer++] = "IDENFR " + buff; // 拼接buff是标识符的名字
                return IDENFR;
            } else { // tmp是保留字，那么它已经在isRemain 里面放进去了
                //cout << "This is remain word " << tmp << endl;
                return tmp;     // 直接返回是哪一种类型的保留字TOKEN
            }
        }
        else if (isdigit(ch)) { // 0 1 2 3 4 5 6 7 8 9
            string numstr = "";
            do {
                numstr = numstr + ch;
                ch = infile.get();
            }while (isdigit(ch));
            // 这里会多读一个非数字，要退回，如果是字母，01xxz不能做为标识符，报错，然后继续正常当成标识符读入
            if (isalpha(ch) || isUnderline(ch)) {
                printf("Line %d : alpha or underline cannot follow INTCON\n", curLine);
                error(unvalid_char);
                string buff = "";
                while(isalnum(ch) || isUnderline(ch)){
                    buff = buff + ch;
                    ch = infile.get();
                }
                infile.seekg(-1, ios::cur);
                idenfr = buff;
                buffer[buf_pointer++] = "IDENFR " + buff; // 拼接buff是标识符的名字
                return IDENFR;
            } else {
                cur_int_con = stoi(numstr); // 转换成10进制整数, update最新的无符号整数
                if (numstr[0] == '0' && numstr.size() > 1) {
                    printf("Line %d: INTCON 不能有前导0\n", curLine);
                    error(unvalid_char); // 报不符合词法的错误
                }
            }
            infile.seekg(-1, ios::cur);
            buffer[buf_pointer++] = "INTCON " + numstr;
            return INTCON;
        }
        else if (ispunct(ch)) { // is punctuation : , . * - + = _ { [ ( ) ] } ' "
            // 加减乘除
            if (ch == '+') {
                buffer[buf_pointer++] = "PLUS +";
                return PLUS;
            } else if (ch == '-') {
                buffer[buf_pointer++] = "MINU -";
                return MINU;
            } else if (ch == '*') {
                buffer[buf_pointer++] = "MULT *";
                return MULT;
            } else if (ch == '/') {
                buffer[buf_pointer++] = "DIV /";
                return DIV;
            } else if (ch == '(') {
                buffer[buf_pointer++] = "LPARENT (";
                return LPARENT;
            } else if (ch == ')') {
                buffer[buf_pointer++] = "RPARENT )";
                return RPARENT;
            } else if (ch == '[') {
                buffer[buf_pointer++] = "LBRACK [";
                return LBRACK;
            } else if (ch == ']') {
                buffer[buf_pointer++] = "RBRACK ]";
                return RBRACK;
            } else if (ch == '{') {
                buffer[buf_pointer++] = "LBRACE {";
                return LBRACE;
            } else if (ch == '}') {
                buffer[buf_pointer++] = "RBRACE }";
                return RBRACE;
            } else if (ch == '"') { // 引号, 文法中字符串是不包括引号的哦
                // 读一句字符串常量 直到遇见下一个 '"'
                string astring = "";
                do {
                    ch = infile.get();
                    if (ch == '"') {
                        break; // no need for retreat
                    } else {
                        if(ch == '\n') {
                            printf("Line %d : no \" after string.\n", curLine);
                            error(unvalid_char);
                            curLine++;
                            break; // 都换行了还没结束，字符串缺少引号，后面递归分析会报错缺少分号？
                        }
                        else if (!(ch == 32 || ch == 33 || (ch >=35 && ch <= 126))) {
                            printf("Line %d : %c unvalid. StringCon can only contain 32,33,35-126 ASCII\n", curLine, ch);
                            error(unvalid_char);
                        }
                        astring = astring + ch; // 有完整"依然可以正常继续
                        if (ch == '\\') {
                            astring = astring + ch; // 有转义字符多输出一个斜杠
                        }
                    }
                }while (1);
                cur_str_con = astring;  // 更新当前最新的字符串常量
                buffer[buf_pointer++] = "STRCON " + astring;
                return STRCON;
            }
            else if (ch == '\'') { // + * a-z A-Z 0-9
                ch = infile.get();
                if (ch == '+' || ch == '-' || ch == '*' || ch == '/'
                    || isalnum(ch) || isUnderline(ch)) {
                    string charcon = "CHARCON ";
                    buffer[buf_pointer++] = charcon + ch;
                    cur_char_con = ch;
                    ch = infile.get(); // 先读入后一个引号，否则return 后面的语句没办法进行
                    if (ch != '\'') {
                        printf("Line %d : missing ' in CHARCON.\n", curLine);
                        error(unvalid_char);
                        infile.seekg(-1, ios::cur);// 回退一个保证语法分析正确进行
                    }
                    return CHARCON;
                } else {
                    printf("Line %d : %c unvalid char in CHARCON.\n", curLine, ch);
                    error(unvalid_char);
                    ch = infile.get(); // 先读入后一个引号，否则 return 后面的语句没办法进行
                    return CHARCON; // 非法字符，正常进行
                }
            } else if (ch == '>' || ch == '<' || ch == '=') {
                // 大于 小于 等于 >=, <=, ==, > , < , !=
                char tmp = ch;
                ch = infile.get();
                if (ch == '=') { // <=, >=, ==
                    switch (tmp) {
                        case '<':
                            buffer[buf_pointer++] = "LEQ <=";
                            return LEQ;   // <=
                            break;
                        case '>':
                            buffer[buf_pointer++] = "GEQ >=";
                            return GEQ;   // >=
                            break;
                        case '=':
                            buffer[buf_pointer++] = "EQL ==";
                            return EQL;   // ==
                            break;
                        default:
                            break;
                    }
                } else { // < , >, =
                    infile.seekg(-1, ios::cur);
                    switch (tmp) {
                        case '<':
                            buffer[buf_pointer++] = "LSS <";
                            return LSS;
                            break;
                        case '>':
                            buffer[buf_pointer++] = "GRE >";
                            return GRE;
                            break;
                        case '=':
                            buffer[buf_pointer++] = "ASSIGN =";
                            return ASSIGN;
                            break;
                        default:
                            break;
                    }
                }
            } else if (ch == '!') {
                ch = infile.get();
                if (ch == '=') { // !=
                    buffer[buf_pointer++] = "NEQ !=";
                    return NEQ;
                }
            } else if (ch == ',') {
                buffer[buf_pointer++] = "COMMA ,";
                return COMMA;
            } else if (ch == ';') {
                buffer[buf_pointer++] = "SEMICN ;";
                return SEMICN;
            }
        }
        else {
            printf("Line %d :unvalid_char %c \n", curLine, ch);
            error(unvalid_char); // continue
        }
        //printf("Current char is %c \n", ch);
    }
    
    return "";
}
// 这个getSym 一次只能返回一个标识符
void clearBuf() {
    //cout << "now buf_pointer is " << buf_pointer << endl;
    int i = 0;
    // 把 buffer里的内容全部倒出来
    //outfile << "now buffer have " << buf_pointer-1 << "lines to clear" << endl;
    for (i = 0; i < buf_pointer; i++) {
        outfile << buffer[i] << endl;
        buffer[i] = "";
    }
    buf_pointer = 0;
}
// <程序> ::= ［<常量说明>]［<变量说明>］{<有返回值函数定义>|<无返回值函数定义>}<主函数>
// 改写
// ［<常量说明>][int | char <标识符>{,<标识符> | }]
void program() {
    if (sym == CONSTTK) {
        //cout << "into const description!" << endl;
        is_defining_con_var = true;
        cons_des();
        is_defining_con_var = false;
    }
    // 上面的常量说明子程序里面多读了一个sym，所以这里直接判断sym
    if(sym == INTTK || sym == CHARTK) { // 注意这个可能是变量说明也可以是函数定义
        is_defining_con_var = true;
        var_des(); // 可有可无，在里面判断是不是 函数定义
        is_defining_con_var = false;
        //cout << "从第一个 var des 出来了, sym == " << sym << endl;
    }
    while (!(sym == INTTK || sym == CHARTK || sym == VOIDTK)) {
        sym = getSym();
    }
    while (sym == INTTK || sym == CHARTK || sym == VOIDTK) {
        if(sym == INTTK || sym == CHARTK) {
            return_func();
        } else if(sym == VOIDTK){ // 不知道下一个到底是无返回函数还是主函数
            sym = getSym();
            if (sym == IDENFR) {  // 已经读进来 void findMin
                string ID_funcName = buffer[buf_pointer-1];     // "IDENFR funcname"
                string real_name = ID_funcName.substr(IDENFR_len + 1); // 从第七个开始取
                curFunc = real_name;    // 更新当前所在函数体
                midCode mid(2, "void", real_name);
                outputMid(mid);
                if (Func_attr.count(real_name) == 1
                    || global_cons.count(real_name) == 1
                    || global_vars.count(real_name) == 1) {
                    cout << "Line :" << curLine << " 重定义函数名 " << real_name << endl;
                    error(double_def_name);
                } else {
                    Func_attribute fa(real_name,0); // 照样正常插入
                    Func_attr.insert(make_pair(real_name,fa));
                }
                sym = getSym();
                non_return_func();
                // 加到 vector 哦～
            } else if (sym == MAINTK) {   // 必然会到这一步
                curFunc = "main";
                midCode mid(2, "void", "main");
                outputMid(mid);
                if (Func_attr.count(curFunc) == 1
                    || global_cons.count(curFunc) == 1
                    || global_vars.count(curFunc) == 1) {
                    cout << "Line :" << curLine << " 重定义main函数名 " << curFunc << endl;
                    error(double_def_name);
                }
                Func_attribute fa("main",0);
                Func_attr.insert(make_pair("main",fa));
                sym = getSym();
                mainProgram();
            }
        }
    }
    buffer[buf_pointer++] = "<程序>";
    clearBuf();
}
void cons_des() { // <常量说明>
    // <常量说明> ::=  const<常量定义>;{ const<常量定义>;}
    while(sym == CONSTTK) {
        sym = getSym();         // 看看下一个是不是 INT / CHAR
        cons_def();             // 交给下一层查看 <常量定义>
        if(sym == SEMICN) {   // 分号匹配成功
            //cout << "常量定义后读到分号" << endl;
            sym = getSym();
        } else {
            cout << "Line : " << last_sym_line << "常量定义后缺少分号，此时读到的sym = " << sym << " " << idenfr << endl;
            error(should_have_semicn);
        }
        continue;
    } // 此时也已经读到了分号的下一个非const的类别码
    buffer[buf_pointer++] = "<常量说明>";   // 由于刚才已经把 预先读的一个sym放到buffer里去了
    // 所以要先 swap 一下最后一个string和倒数第二个string，把 <常量说明>放到分号的后面
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    // 此时 sym 不是 const 了，可能到变量说明了
    
}
void cons_def() { // <常量定义>
    // <常量定义>   ::=   int<标识符>＝<整数>{,<标识符>＝<整数>}
    //                  | char<标识符>＝<字符>{,<标识符>＝<字符>}
    //cout << "is defining ? " << is_defining_con_var << endl;
    string curType;
    if (sym == INTTK) {
        curType = "int";    // int
        sym = getSym();
        if(sym == IDENFR) {
            //cout << "进入常量定义 查重复定义" << endl;
            checkIdenfrDef(INTTK, idenfr, false); // cons false
            sym = getSym();
            if(sym == ASSIGN) {
                sym = getSym();     // 要先预读一个符号, 它会在文件中输出类别码
            } else{
                cout << "no assign signal here !" << endl;
            }
            if (!integer()) {  // integer函数里也会多读一次 sym 的
                printf("Line %d : int 常量后赋值的不是整数\n", curLine);
                error(cons_only_int_or_char); // O 类
            }
            // 存一下整型常量的值
            if (curFunc == "NULL") {
                global_int_cons_value.insert(make_pair(idenfr, stoi(cur_integer)));
            } else { // 局部常量, 这里要改成对应函数的 map
                Func_attr[curFunc].int_cons_value.insert(make_pair(idenfr, stoi(cur_integer)));
                cout << "存局部int常量 " << idenfr << " 的值 " << cur_integer << endl;
            }
            
            // 现在生成中间代码“常量定义 type = 4”
            midCode mid(4, "const", curType, idenfr, cur_integer); // 最后一项应该是当前有符号整数
            outputMid(mid);
        }
        while (sym == COMMA) {    // 逗号说明定义还没有结束
            sym = getSym();
            if(sym == IDENFR) {
                checkIdenfrDef(INTTK, idenfr, false); // cons false
                sym = getSym();
                if(sym == ASSIGN) {
                    sym = getSym();     // 要先预读一个符号,有可能读进来 +  -
                }
                if (!integer()) {
                    printf("Line %d : int 常量后赋值的不是整数\n", curLine);
                    error(cons_only_int_or_char);
                } // unsigned 函数里也会多读一次 sym 的, 所以 error之后不用 getsym
                // 存一下整型常量的值
                if (curFunc == "NULL") {
                    global_int_cons_value.insert(make_pair(idenfr, stoi(cur_integer)));
                } else {
                    Func_attr[curFunc].int_cons_value.insert(make_pair(idenfr, stoi(cur_integer)));
                    cout << "存局部int常量 " << idenfr << " 的值 " << cur_integer << endl;
                }
                // 现在生成中间代码“常量定义 type = 4”, 这个中间代码好像没有卵用
                midCode mid(4, "const", curType, idenfr, cur_integer); // 最后一项应该是当前有符号整数
                outputMid(mid);
            }
        } // 最后 integer中 sym 读到了 ; 分号 这个定义就结束了
    } else if(sym == CHARTK) {
        curType = "char";
        sym = getSym();
        if(sym == IDENFR) {
            checkIdenfrDef(CHARTK, idenfr, false); // cons false
            sym = getSym();
            if(sym == ASSIGN) {
                sym = getSym();     // 为什么要先预读一个符号
            }
            if(sym == CHARCON) {  // 判断字符不需要子程序
                sym = getSym();
            } else {
                printf("Line %d : char常量定义不是字符\n", curLine);
                error(cons_only_int_or_char);
                sym = getSym();
            }
            // 存一下char常量的值
            if (curFunc == "NULL") {
                global_char_cons_value.insert(make_pair(idenfr, cur_char_con));
            } else {
                Func_attr[curFunc].char_cons_value.insert(make_pair(idenfr, cur_char_con));
                cout << "存局部char常量 " << idenfr << " 的值 " << cur_char_con << endl;
            }
            // 生成代码“常量定义 type = 4”
            string ch_con;
            ch_con = ch_con + cur_char_con;
            midCode mid(4,"const", curType, idenfr, ch_con); // 放进当前单个字符
            outputMid(mid);
        }
        while (sym == COMMA) {    // 逗号说明定义还没有结束
            sym = getSym();
            if(sym == IDENFR) {
                checkIdenfrDef(CHARTK, idenfr, false); // cons false
                sym = getSym();
                if(sym == ASSIGN) {
                    sym = getSym();     // 为什么要先预读一个符号
                }
                if(sym == CHARCON) {  // 判断字符不需要子程序
                    sym = getSym(); // 最后由于读到 ; 跳出循环
                } else {
                    printf("Line %d : char常量定义不是字符\n", curLine);
                    error(cons_only_int_or_char);
                    sym = getSym();     // 没有关于字符的递归下降函数，不会多getsym，所以要手动继续
                }
                // 存一下char常量的值
                if (curFunc == "NULL") { // 全局
                    global_char_cons_value.insert(make_pair(idenfr, cur_char_con));
                } else {
                    Func_attr[curFunc].char_cons_value.insert(make_pair(idenfr, cur_char_con));
                    cout << "存局部char常量 " << idenfr << " 的值 " << cur_char_con << endl;
                }
                string ch_con;
                ch_con = ch_con + cur_char_con;
                midCode mid(4,"const", curType, idenfr, ch_con); // 放进当前单个字符
                outputMid(mid);
            }
        }
    }
    buffer[buf_pointer++] = "<常量定义>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
bool integer() {   // 整数（带符号）
    // <整数> ::= ［＋｜－］<无符号整数>
    string symbol = "";
    if(sym == PLUS || sym == MINU) {
        symbol = (sym == PLUS)? "+" : "-";
        sym = getSym(); // 此时应该返回的是整型常量了
    }
    bool is_integer = unsigned_int();   // 实质上为递归下降
    
    buffer[buf_pointer++] = "<整数>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    
    cur_integer = symbol + to_string(cur_int_con);  // 如果前面是正负号的话，这个字符串是可以用stoi 解析为 int 的
    return is_integer;
}
bool unsigned_int() {   // 无符号整数
    // <无符号整数> ::= <非零数字>｛<数字>｝| 0
    bool is_int = true;
    if (sym == INTCON) {
        cout << cur_int_con << endl;
        sym = getSym();     // 此时返回的可能是 , 逗号也有可能是 ; 分号表明这一行的结束
    } else { // 不是无符号整数
        cout << "这里不是无符号整数 , sym = " << sym << endl;
        is_int = false;
        sym = getSym();  // 读错，要继续词法分析
    }
    // 记得swap把分号放到后面哦
    buffer[buf_pointer++] = "<无符号整数>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    return is_int;
}

void var_des() { // <变量说明> 这里由于要判断是不是有返回值的函数，要先读入int|char <标识符>
    // <变量说明>  ::= <变量定义>;{<变量定义>;}
    // 进来的时候 sym 就是 int | char
    bool flag = true;
    int loop_time = 0;
    while(sym == INTTK || sym == CHARTK) {
        string var_type = sym;
        sym = getSym(); // 多读一个标识符
        if (sym == IDENFR) { // 尚不能确认是不是数组/函数
            sym = getSym();
            if(sym == LPARENT) {
                // 发现左括号，及时止损，先立刻存一下这个 有返回值函数的名字！！！！
                // 先把函数名字加入 Func_attr map
                string ID_funcName = buffer[buf_pointer-2];     // "IDENFR funcname"
                string real_name = ID_funcName.substr(IDENFR_len + 1); // 从第七个开始取
                curFunc = real_name; // 更新函数局域
                if (Func_attr.count(real_name) == 1
                    || global_cons.count(real_name) == 1
                    || global_vars.count(real_name) == 1) {
                    cout << "Line :" << curLine << " 重定义函数名 " << real_name << endl;
                    error(double_def_name);
                } // 重定义函数要不要插入func_attr
                // 然后判断一下返回值的类型，int = 1, char = 2
                string func_type = buffer[buf_pointer-3];
                if (func_type[0] == 'I') { // INTTK
                    Func_attribute fa(real_name,1);
                    Func_attr.insert(make_pair(real_name,fa));
                    midCode mid(2, "int", real_name); // 生成函数声明头中间代码
                    outputMid(mid);
                } else if (func_type[0] == 'C') { // CHARTK
                    Func_attribute fa(real_name,2);
                    Func_attr.insert(make_pair(real_name,fa));
                    midCode mid(2, "char", real_name); // 生成函数声明头中间代码
                    outputMid(mid);
                }
                if(loop_time >= 1) { // 后来检测到了 带 有返回值函数定义，若loop_time == 0说明根本没有变量定义
                    // int a[2], abc; int getMin( 已经读到这里了
                    // 那么buffer中的 <变量说明> 需要放在 倒数第三个 sym 之前，然后再输出
                    int i;
                    for (i = buf_pointer; i > buf_pointer - 3; i--) {
                        buffer[i] = buffer[i - 1];
                    }
                    buffer[buf_pointer - 3] = "<变量说明>";
                    buf_pointer++;
                }
                flag = false;
                return_func_begin_para();
                break;
            } else if(sym == SEMICN) { // 已经得到分号，要在分号之前输出 <变量定义>
                // 定义一个变量！！
                checkIdenfrDef(var_type, idenfr, true); //  这个函数里面变量分配地址
                buffer[buf_pointer++] = "<变量定义>";
                // 生成中间代码, type = 4
                string curType = (var_type == INTTK)? "int" : "char";
                midCode mid(4, "var", curType, idenfr, to_string(0));   // 单个变量，数组大小为0
                outputMid(mid);
                
                swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
                sym = getSym();
                continue;
            } else { // 可能是 , 更多变量，[ 一个数组
                var_def(var_type);  // 这个是从半道儿开始的 <变量定义> 判别函数
                // 当前预读到了一个分号 ;
                if (sym == SEMICN) {
                    sym = getSym(); // 再预读下一个看看是不是 int | char
                } else {
                    printf("Line %d: 变量定义后缺少分号\n", last_sym_line);
                    error(should_have_semicn);
                }
            }
        }
        loop_time++;
    }
    if (!flag && loop_time == 0) {
        // 进这个函数 var_des 就算废了
    } else { // 至少已经成功的经过了一轮变量定义
        if(flag) { // 后来没有检测到 带 有返回值函数定义，只是多读了一个分号后面的东西
            buffer[buf_pointer++] = "<变量说明>";
            swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
        }
    }
}
void var_def(string type) {  // <变量定义> 是不能单独存在的，必然是隶属于<变量说明>的
    // <变量定义>  ::= (int | char) (<标识符> ['['<无符号整数>']'])
    //                  {,(<标识符> ['['<无符号整数>']' ])}
    // 有可能从 , [ 开始
    // 默认已经读了 int | char <标识符>, 或者 int | char <标识符> [ 或者 int | char <标识符>;
    if (sym == COMMA) {
        // 既然是逗号，先定义逗号前面的那个标识符
        checkIdenfrDef(type, idenfr, true);
        // 生成代码，存变量  type = 4
        string curType = (type == INTTK)? "int" : "char";
        midCode mid(4, "var", curType, idenfr, to_string(0)); // 是单个变量，数组大小为0
        outputMid(mid);
        while (sym == COMMA) {  // 然后再继续往下
            sym = getSym();
            name_or_array(type);    // name_or_array里面生成函数
        }
    } else if(sym == LBRACK){ // [，说明是数组且有大小
        sym = getSym();
        if (unsigned_int()) { // 定义数组并存大小
            checkIdenfrDef(type, idenfr, true, cur_int_con); // 这个函数里会分配数组大小的栈
            // 生成代码，存变量  type = 4
            string curType = (type == INTTK)? "int" : "char";
            midCode mid(4, "var", curType, idenfr, to_string(cur_int_con)); // 数组大小cur_int_con
            outputMid(mid);
            // 忘记分配地址了？
        } else {
            printf("Line %d : 定义变量数组下标不是 无符号整数\n",curLine);
            error(array_index_int);
        }
        if (sym == RBRACK) { // ]
            sym = getSym();     // int a[3],
        } else {
            printf("Line %d : 数组下标后面缺少']'\n", curLine);
            error(should_have_rbrack);
        }
        while (sym == COMMA) {
            sym = getSym();
            name_or_array(type); // 里面会存数组或变量元素的
        }
    } // 否则是分号 不做处理
    // 它也会预读一个sym(是 ; ) 所以需要 swap
    buffer[buf_pointer++] = "<变量定义>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    //clearBuf();
}
void name_or_array(string type) {
    //cout << "进入变量定义的name_or_array " << endl;
    if (sym == IDENFR) {
        sym = getSym();
        if (sym == LBRACK) { // 是数组
            sym = getSym();
            if (unsigned_int()) { // 定义一个是数组的变量
                checkIdenfrDef(type, idenfr, true, cur_int_con);
                // 生成代码，存变量  type = 4
                string curType = (type == INTTK)? "int" : "char";
                midCode mid(4, "var", curType, idenfr, to_string(cur_int_con)); // 数组大小cur_int_con
                outputMid(mid);
            } else {
                printf("Line %d : 变量定义中数组下标不是unsigned int\n", curLine);
                error(array_index_int);
            }
            if (sym == RBRACK) { // name[4] 匹配成功
                // 读到这里的时候就知道是不是数组了，调用 idenfr 当前的标识符就可以知道当前的变量名
                sym = getSym();     // 可能读进来 , ;
            } else {
                printf("Line %d : 数组下标后面缺少']'\n", curLine);
                error(should_have_rbrack);
            }
        } else { // 不是数组,存下变量自动查重
            checkIdenfrDef(type, idenfr, true); // size 默认为0
            // 生成代码，存变量  type = 4
            string curType = (type == INTTK)? "int" : "char";
            midCode mid(4, "var", curType, idenfr, to_string(0)); // 数组大小cur_int_con
            outputMid(mid);
        }
    }
}
void return_func() { // <有返回值函数定义>
    // ::= <声明头部>'('<参数表>')' '{'<复合语句>'}'
    // int findMin(int x1, int x2, char name) { <复合语句> }
    dec_head();
    if(sym == LPARENT) {  // (
        //cout << "在return_func里的 读到 '(' " << endl;
        sym = getSym();    // 读进一个 INT / CHAR
        para_table(curFunc);     // 参数表 （其实这个curFunc参数传进去没有意义）
        if (sym == RPARENT) {  // ) 参数表右括号匹配完毕
            sym = getSym();
        } else {
            printf("Line %d : 参数表后缺少')' \n", curLine);
            error(should_have_rparent);
        }
        if (sym == LBRACE) {
            sym = getSym();
            //cout << "即将进入有返回值函数定义的 复合语句 " << endl;
            compound_statement();   // <复合语句>
            if (sym == RBRACE) {
                sym = getSym(); // 继续预读
            }
        }
    }
    buffer[buf_pointer++] = "<有返回值函数定义>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void return_func_begin_para() { // 从<标识符>后，左括号开始的开始的有返回值函数定义
    // 特制的：'('<参数表>')' '{'<复合语句>'}'
    // <参数表> int name1, char name2, int num
    // 头部已经错过了，这个时候要先添加 <声明头部> 放在 ( 的前面
    buffer[buf_pointer++] = "<声明头部>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    
    if(sym == LPARENT) {  // (
        sym = getSym();    // 读进一个 INT / CHAR
        para_table(curFunc);       // 参数表
        if (sym == RPARENT) {  // )
            sym = getSym();
        } else {
            printf("Line %d : 参数表后缺少')' \n", curLine);
            error(should_have_rparent); // 在上面断掉的地方继续
        }
        if (sym == LBRACE) {
            sym = getSym();
            compound_statement();   // <复合语句>
            if (sym == RBRACE) {
                sym = getSym(); // 继续预读
            }
        }
    }
    buffer[buf_pointer++] = "<有返回值函数定义>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void dec_head() {   // 声明头部
    // <声明头部> ::=  int<标识符> | char<标识符>
    if(sym == INTTK || sym == CHARTK) {
        int ret_type = (sym == INTTK)? 1 : 2;
        sym = getSym();
        if(sym == IDENFR) {
            // 匹配成功，赶紧存一下该函数的名字 buffer[buf_pointer-1] 是刚刚存下来的 "IDENFR 函数名"
            string ID_funcName = buffer[buf_pointer-1];     // "IDENFR funcname"
            string real_name = ID_funcName.substr(IDENFR_len + 1); // 从第七个开始取
            curFunc = real_name;    // 其实可以直接调用idenfr
            
            if (Func_attr.count(real_name) == 1
                || global_cons.count(real_name) == 1
                || global_vars.count(real_name) == 1) {
                cout << "Line :" << curLine << " 重定义函数名 " << real_name << endl;
                error(double_def_name);
            } else { // 如果函数名与前面的全局常量或者变量 重复了，那么，算未定义
                Func_attribute fa(real_name, ret_type);
                Func_attr.insert(make_pair(real_name,fa));
                //cout << "添加新函数 " << real_name << " 此时函数个数： " << Func_attr.size() << endl;
            }
            // 函数声明头，生成中间代码 type = 2
            string curType = (ret_type == 1)? "int" : "char";
            midCode mid(2, curType, idenfr); // 函数名字其实可以直接调用idenfr
            outputMid(mid);
            sym = getSym();     // 又读进来了一个 左括号 (
        }
    }
    buffer[buf_pointer++] = "<声明头部>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void para_table(string curFunc) {  // 参数表，不会有数组元素的形式
    // <参数表> ::=  (int | char)<标识符>{,(int | char))<标识符>}| <空>
    // 传进来的 curFunc 是函数名，现在开始定义这个函数的参数表
    if(sym == INTTK || sym == CHARTK) {
        string type = sym;
        sym = getSym();
        if(sym == IDENFR) { // 得到参数名
            Func_attr[curFunc].addPara(type, idenfr); // 自带查重
            // 中间代码生成
            string curType = (type == INTTK)? "int" : "char";
            midCode mid(4, "para", curType, idenfr, to_string(0));   // 单个变量，数组大小为0
            outputMid(mid);
            Func_attr[curFunc].allocVarAddr(idenfr, 0); // 函数的参数当作普通变量也分配地址
            sym = getSym();
        }
    }
    while (sym == COMMA) {
        sym = getSym();
        if(sym == INTTK || sym == CHARTK) {
            string type = sym;
            sym = getSym();
            if(sym == IDENFR) {
                Func_attr[curFunc].addPara(type, idenfr);
                string curType = (type == INTTK)? "int" : "char";
                midCode mid(4, "para", curType, idenfr, to_string(0));   // 单个变量，数组大小为0
                outputMid(mid); // 生成代码
                Func_attr[curFunc].allocVarAddr(idenfr, 0);
                sym = getSym(); // 预读一个 ) 右括号
            }
        }
    }
    // 假如是空的话就会直接跳到这里来
    buffer[buf_pointer++] = "<参数表>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void non_return_func() {  // <无返回值函数定义>
    // <无返回值函数定义> ::= void<标识符>'('<参数表>')''{'<复合语句>'}'
    // 在最上层的时候已经预先读了一个左括号
    if (sym == LPARENT) {
        sym = getSym(); // get 一个 int | char
        para_table(curFunc);
        if (sym == RPARENT) {  // )
            sym = getSym();
        } else {
            printf("Line %d : 参数表后缺少')' \n", curLine);
            error(should_have_rparent);
        }
        if (sym == LBRACE) {  // {
            sym = getSym();
            compound_statement();   // <复合语句>
            if (sym == RBRACE) {
                sym = getSym(); // 继续预读
            }
        }
    }
    buffer[buf_pointer++] = "<无返回值函数定义>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void mainProgram() {  //正常开始的 <主函数>
    // <主函数> ::= void main‘(’‘)’ ‘{’<复合语句>‘}’ 实际上 从 void main 后面开始判断
    if (sym == LPARENT) {
        sym = getSym();
        if (sym == RPARENT) {
            sym = getSym();
        } else {
            printf("Line %d : main'(' 后缺少')' \n", curLine);
            error(should_have_rparent); // 在断掉的地方继续
        }
        if (sym == LBRACE) {
            sym = getSym();
            compound_statement();   // <复合语句>
            if (sym == RBRACE) {
                // 主函数的 } 结束了，不会再预读了
            }
        }
    }
    buffer[buf_pointer++] = "<主函数>";    // 也不用调换位置
}
void compound_statement() {  //  <复合语句> 只会在函数定义和主函数和语句列里出现，所以结束了后面就是 '}'
    // <复合语句> ::=  ［<常量说明>］［<变量说明>］<语句列>
    // 预读了 const ? int | char ? 如果不是！就是 <语句列>
    have_return_stat = false; // 开始时没有返回语句
    if (sym == CONSTTK) {
        //cout << "进入复合语句的常量说明" << endl;
        is_defining_con_var = true;
        cons_des();
        is_defining_con_var = false;
    }
    if (sym == INTTK || sym == CHARTK) {
        //cout << "进入复合语句的变量说明" << endl;
        is_defining_con_var = true;
        var_des();
        is_defining_con_var = false;
    }
    is_defining_con_var = false; // 定义阶段结束
    // 此时可能已经读到语句的第一个 token 了！！
    statement_column(); // 它会多读一个 sym，但是在 statement 里面会提前换好
    // statement 中已经可以决定有没有返回语句了
    // 语句列会包含大括号，所以这里报错missing 是在 '}' 所在行
    if (Func_attr[curFunc].getType() != 0 && !have_return_stat) {
        printf("Line %d : 有返回值的函数缺少 return 语句\n", curLine);
        error(missing_return);
    }
    buffer[buf_pointer++] = "<复合语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    //clearBuf();     // 这时候终于可以 clear 了
}
void statement_column() {   // <语句列>
    // <语句列> ::= ｛<语句>｝
    // 就是很多很多语句排在一起啦
    while (is_Statement_head()) {
        statement();
        // statement会预读，<语句> 也没有前缀 所以这里可以不用管他
    }
    // statement 会多读一个 sym 此时已经不是语句头了，会读到分号后面的东西
    buffer[buf_pointer++] = "<语句列>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    // clearBuf(); 后面可能还要输出 <复合语句> 所以不能先输出下一个 sym
}
bool is_Statement_head(){
    return (sym == IFTK || sym == WHILETK || sym == DOTK || sym == FORTK
            || sym == LBRACE || sym == IDENFR || sym == SCANFTK
            || sym == PRINTFTK || sym == RETURNTK || sym == SEMICN);
    // 注意 空语句的 ; 分号也是语句头！！
}
void statement() {  // <语句> 他们的first集都不重复耶！！开心！
    /*  <语句> ::= <条件语句>｜<循环语句>| '{'<语句列>'}'| <有返回值函数调用语句>;
     *          |<无返回值函数调用语句>;｜<赋值语句>;｜<读语句>;｜<写语句>;｜<空>;|<返回语句>;
     */
    // 注意 普通语句是要 ; 分号结尾的！！！
    // 条件： if
    if (sym == IFTK) {
        condition_stat();   // 它会预读
    } else if (sym == WHILETK || sym == DOTK || sym == FORTK) {
        // 循环：while / do / for 开头
        cir_stat();         // 它会预读
    } else if (sym == LBRACE) { // { 开头 语句列
        sym = getSym();
        statement_column();
        if (sym == RBRACE) {    // 读到 } 才算语句完成
            sym = getSym(); // 预读
        }
    } else if (sym == IDENFR) {
        // 有无返回值函数调用一模一样！！！！开头也是 IDENFR 要建立一个表来存储这两种函数的名字
        string tmp_idenfr = idenfr;
        sym = getSym();
        if (sym == ASSIGN || sym == LBRACK) { // name = , name[
            checkIdenfrDef("NULL", tmp_idenfr, true);
            assign_stat();  // 注意，这是从标识符后面开始的，可能是 num = 或者 nums[，仅此一处调用 赋值语句
        } else if (sym == LPARENT) { // 函数
            call_stat(); // 可能有 也可能没有返回值
        }
        if (sym == SEMICN) {    // 分号结束后多读一个
            sym = getSym();
        } else {
            printf("Line %d : 赋值语句或函数调用语句后缺少分号\n", last_sym_line);
            error(should_have_semicn);
        }
    } else if (sym == SCANFTK) {
        read_stat();
        if (sym == SEMICN) {    // 分号结束后多读一个
            sym = getSym();
        } else {
            printf("Line %d : scanf语句后缺少分号\n", last_sym_line);
            error(should_have_semicn);
        }
    } else if (sym == PRINTFTK) {
        write_stat();
        if (sym == SEMICN) {    // 分号结束后多读一个
            sym = getSym();
        } else {
            printf("Line %d : printf语句后缺少分号\n", last_sym_line);
            error(should_have_semicn);
        }
    } else if (sym == RETURNTK) {
        return_stat();
        if (sym == SEMICN) {    // 分号结束后多读一个
            sym = getSym();
        } else {
            printf("Line %d : return 语句后缺少分号\n", last_sym_line);
            error(should_have_semicn);
        }
    } else if (sym == SEMICN) { // 直接读到了分号，这是个空语句
        sym = getSym(); // 那么预读下面一个
    } else {
        printf("Line %d : 语句分析错误\n", curLine);
    }
    buffer[buf_pointer++] = "<语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    //clearBuf(); // 语法分析要删掉
}
void condition_stat() {  // 条件语句
    // <条件语句>  ::= if '('<条件>')'<语句>［else<语句>］
    pair<string, string> labels;
    if (sym == IFTK) {
        labels = genLabel("if");
        sym = getSym();
        if (sym == LPARENT) {
            sym = getSym();
            condition();
            midCode mid(3, "BZ", getCurTempReg(), labels.first); // 不满足条件跳转BZ if_label1
            outputMid(mid);
            if (sym == RPARENT) {
                sym = getSym();
            } else {
                printf("Line %d : 条件后缺少 ')'\n", curLine);
                error(should_have_rparent);
            }
            statement();
            // 先生成 goto label2
            midCode mid1(2, "goto", labels.second);
            outputMid(mid1);
            midCode mid2(8, labels.first); // 放第一个标签
            outputMid(mid2);
            if (sym == ELSETK) {
                sym = getSym();
                statement();
            }
            // 生成末尾标签
            midCode mid3(8, labels.second);
            outputMid(mid3);
        }
    }
    buffer[buf_pointer++] = "<条件语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
void condition() { // <条件>
    // <条件> ::=  <表达式><关系运算符><表达式>｜<表达式> //表达式为0条件为假，否则为真
    // 关系运算符不用输出 < > == <= >= !=  // <express调用完了就可以知道cur_exp_type是什么类型的了>
    string arg1 = express();
    string arg2;
    if (cur_exp_type == 2) {
        printf("Line %d : 当前条件中的 关系运算符前 表达式不是 int 类型的 type = %d\n", curLine, cur_exp_type);
        error(condition_type_wrong);
    }
    string compare_op = "";
    if (sym == LSS || sym == LEQ || sym == GRE
        || sym == GEQ || sym == EQL || sym == NEQ) {
        compare_op = sym;
        sym = getSym();
        arg2 = express();
        if (cur_exp_type == 2) {
            printf("Line %d : 当前条件中 关系运算符后 的表达式不是 int 类型的 type = %d\n", curLine, cur_exp_type);
            error(condition_type_wrong);
        }
    }
    buffer[buf_pointer++] = "<条件>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    if (compare_op == "") { // 条件只有一个表达式
        midCode mid(5, genTempReg(), arg1, "", ""); // 直接把 arg1 的值赋给寄存器
        outputMid(mid);
    } else {
        midCode mid(5, genTempReg(), arg1, compare_op, arg2); // 二者比较的结果赋给寄存器
        outputMid(mid);
    }
    Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
    // 给比较结果的中间变量分配一个可存的地址
}
void cir_stat() {   // 循环语句
    /*  while '('<条件>')'<语句>
     *  | do<语句>while '('<条件>')'
     *  |for'('<标识符>＝<表达式>;<条件>;<标识符>＝<标识符>(+|-)<步长>')'<语句>
     */
    // 一般 do 后面要跟 {语句列} 注意大括号的判别
    if (sym == WHILETK) {
        pair<string, string> labels = genLabel("while");
        midCode mid(8, labels.first);
        outputMid(mid);
        sym = getSym();
        if (sym == LPARENT) {
            sym = getSym();
            condition();  // BZ newTemoReg while_end1   # 不满足条件则跳转到end
            midCode mid1(3, "BZ", getCurTempReg(), labels.second);
            outputMid(mid1);
            if (sym == RPARENT) {
                sym = getSym();
                statement();    // 它会预读
            } else {
                printf("Line %d : while条件后缺少) \n", curLine);
                error(should_have_rparent);
                statement();    // 它会预读
            }
            midCode mid2(2, "goto", labels.first);
            outputMid(mid2); // 先无条件跳转到头
            midCode mid3(8, labels.second);
            outputMid(mid3);
        }
    }
    else if (sym == DOTK) {
        pair<string, string> labels = genLabel("do");
        midCode mid(8, labels.first);
        outputMid(mid);
        sym = getSym();
        statement();
        if (sym == WHILETK) {
            sym = getSym();
        } else {
            printf("Line %d : do -while语句缺少 while \n", curLine);
            error(missing_while);
        }
        if (sym == LPARENT) { // '('<条件>')'
            sym = getSym();
            condition();
            midCode mid1(3, "BNZ", getCurTempReg(), labels.first); // 满足条件跳回头部
            outputMid(mid1);
            if (sym == RPARENT) {
                sym = getSym(); // 预读
            } else {
                printf("Line %d : do while条件后缺少 ')'\n", curLine);
                error(should_have_rparent); // 它已经读到下一个其他东西的了，这里不用再预读了
            }
        }
        midCode mid2(8, labels.second);
        outputMid(mid2); // 这个放不放都无所谓
    }
    else if (sym == FORTK) {
        // for'('<标识符>＝<表达式>;<条件>;<标识符>＝<标识符>(+|-)<步长>')'<语句>
        string tmp_idenfr;  // 记下，与后面比较是不是归纳变量
        pair<string, string> labels = genLabel("for");
        sym = getSym();
        if (sym == LPARENT) {
            sym = getSym();
            if (sym == IDENFR) {
                checkIdenfrDef("NULL", idenfr, true);
                tmp_idenfr = idenfr;
                sym = getSym();
            }
        }
        if (sym == ASSIGN) {    // 第一条赋值
            sym = getSym();
            string tmp_exp = express();
            // 生成第一条中间代码
            midCode mid(2, tmp_idenfr, tmp_exp);
            outputMid(mid);
        }
        if (sym == SEMICN) {    // 分号，中间的条件
            sym = getSym();
            midCode mid(8, labels.first); // for_1_begin:
            outputMid(mid);
            condition();  // <条件>;会生成中间代码，然后生成不满足则转到末尾 BZ for_end1
            midCode mid1(3, "BZ", getCurTempReg(), labels.second);
            outputMid(mid1);
        } else {
            printf("Line %d : for 语句中缺少分号\n", last_sym_line);
            error(should_have_semicn);
            midCode mid(8, labels.first); // for_1_begin:
            outputMid(mid);
            condition();
            midCode mid1(3, "BZ", getCurTempReg(), labels.second);
            outputMid(mid1);
        }
        if (sym == SEMICN) {    // 分号
            sym = getSym();
        } else {
            printf("Line %d : for 语句中缺少分号\n", last_sym_line);
            error(should_have_semicn);
        }
        string conclude_var1,conclude_var2;
        if (sym == IDENFR) {
            checkIdenfrDef("NULL", idenfr, true);
            conclude_var1 = idenfr;
            sym = getSym();
        }
        if (sym == ASSIGN) {
            sym = getSym();
        }
        if (sym == IDENFR) {
            checkIdenfrDef("NULL", idenfr, true);
            conclude_var2 = idenfr;
            sym = getSym();
        }
        string op; // 这个 op 正确文法下一定会被初始化
        string step;  // 步长这里会有归纳变量，归纳变量可以考虑不要存到内存里
        if (sym == PLUS || sym == MINU) {
            op = (sym == PLUS)? "+" : "-";
            sym = getSym();
            step_length();  // 步长，会预读后面de ')'
            step = to_string(cur_int_con);
            if (sym == RPARENT) {
                sym = getSym();
                statement();  // 会预读
            } else {
                printf("Line %d : 步长后面缺少 ')'\n",curLine);
                error(should_have_rparent);
                statement();
            }
        }
        if (concludeVarMap.size() <= 4 ) { // 归纳变量最多只能占用5个
            // 如果两个归纳变量相同，存到 concludeVarMap, tregNo 从0开始的
            if (concludeVarMap.count(conclude_var1) == 0
                && Func_attr[curFunc].findArgumentPos(conclude_var1) == 0) { // 没有这个名字，那么加进去分配
                string regName = "$t" + to_string(tregNo);
                tmpRegisterGroup[tregNo] = 1;   // 直接分配临时寄存器
                concludeVarMap.insert(make_pair(conclude_var1, regName));
                tregNo++;
            }
            if (concludeVarMap.count(conclude_var2) == 0
                && Func_attr[curFunc].findArgumentPos(conclude_var2) == 0) { // 是归纳变量且本函数里没有叫这个名字的参数
                string regName = "$t" + to_string(tregNo);
                tmpRegisterGroup[tregNo] = 1;   // 直接分配临时寄存器
                concludeVarMap.insert(make_pair(conclude_var2, regName));
                tregNo++;
            }
        }
        
        // 给归纳变量最后一步生成midCode , BNZ 满足条件则跳转回 begin label
        // 12.8 修改：这里不用分两步走
        midCode mid(1, conclude_var1, conclude_var2, op, step);
        outputMid(mid);

        midCode mid2(2, "goto", labels.first);
        outputMid(mid2);
        midCode mid3(8, labels.second);
        outputMid(mid3);
    }
    
    buffer[buf_pointer++] = "<循环语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    //clearBuf();
}
void step_length() {  // <步长>
    // <步长>::= <无符号整数>
    if (unsigned_int()) {
        // do nothing
    } else {
        printf("Line %d : 循环语句中的步长不是无符号整数\n", curLine);
        error(unvalid_char);
    }
    buffer[buf_pointer++] = "<步长>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}
string call_stat() {
    // 进来统一判别函数调用语句！！读到最后有没有return 才知道函数有无返回值
    // <有返回值函数调用语句> ::= <标识符>'('<值参数表>')'
    // 所以必然读到 '(' 才能确定是 函数调用语句， 这里可以放心从 '(' <值参数表> 开始
    string real_name = "";
    bool is_defined = true;
    if (Func_attr.count(idenfr) == 0) {
        cout << "Line : " << curLine << " 调用时尚未定义函数 " << idenfr << endl;
        error(undefined_name);
        is_defined = false;
    }
    if (sym == LPARENT) {
        string ID_funcName = buffer[buf_pointer-2];     // "IDENFR funcname"
        real_name = ID_funcName.substr(IDENFR_len + 1); // 从第七个开始取
        sym = getSym();
        value_para_table(real_name);  // 只有这里调用了值参数表
        if (sym == RPARENT) { // ) 完结！预读
            sym = getSym();
        }
        else {
            printf("Line %d : 函数调用语句后缺少 ')' \n", curLine);
            //cout << "此时 sym =  "<< sym << endl;
            error(should_have_rparent); // 不用预读
        }
    }
    if (is_defined) {
        if (Func_attr[real_name].getType() != 0) { // 有可能是1 或 2
            buffer[buf_pointer++] = "<有返回值函数调用语句>";     //
        } else {
            buffer[buf_pointer++] = "<无返回值函数调用语句>";
        }
        swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    }
    // call func_name 生成中间代码
    midCode mid(2, "call", real_name);
    outputMid(mid);
    string midTmp = "";
    // 无返回值函数调用不要生成 #t1 = ret
    if (Func_attr[real_name].getType() != 0) {
        midTmp = genTempReg();
        midCode midret(2, midTmp, "ret");
        outputMid(midret);
        Func_attr[curFunc].allocVarAddr(midTmp, 0); // 记得给新的中间变量分配存址空间
    }

    return midTmp; // 外面要检测一下 two_elem_assign 如果 result == "", 则不做任何处理
}

void value_para_table(string real_name) { // <值参数表>， real_name 是被调用的函数名字
    // <值参数表> ::= <表达式>{,<表达式>}｜<空>
    // 生成中间代码，来一个push一个
    bool func_is_defined = false;
    if (Func_attr.count(real_name) != 0) {
        func_is_defined = true;
    } else {
        func_is_defined = false;
        printf("值参数表检测 尚未定义函数 ");
        cout << real_name << endl;
    }
    int para_num = 0;
    if (sym != RPARENT) { // 不直接是空的
        para_num++;
        string actual_para = express(); // 无返回值调用语句返回的是 "" 此时不需要push
        if (actual_para != "") {
            midCode mid(2, "push", actual_para);
            outputMid(mid);
        }
        if (func_is_defined) {
            if (Func_attr[real_name].para_table.size() == 0) {
                cout << real_name << " 的值参数表是空的" << endl;
            } else {
                if (Func_attr[real_name].para_table[para_num-1].second==INTTK && cur_exp_type != 1) {
                    printf("Line %d : 当前参数应为整型而传进来了char类型\n", curLine);
                    error(para_type_wrong);
                } else if (Func_attr[real_name].para_table[para_num-1].second==CHARTK
                           && cur_exp_type != 2){
                    printf("Line %d : 当前参数应为char类型而传进来了整型\n", curLine);
                    error(para_type_wrong);
                }
            }
        } else {
            printf("Line %d : 该函数名没有被定义，不给检查参数表\n", curLine);
        }
        while (sym == COMMA) {
            para_num++;
            sym = getSym();
            string actual_para = express(); //  会读到 ')'
            if (actual_para != "") {
                midCode mid(2, "push", actual_para);
                outputMid(mid);
            }
            if (para_num <= Func_attr[real_name].para_table.size()) {
                if (func_is_defined
                    && Func_attr[real_name].para_table[para_num-1].second==INTTK
                    && cur_exp_type != 1) {
                    printf("Line %d : 当前参数应为整型而传进来了char类型\n", curLine);
                    error(para_type_wrong);
                } else if (func_is_defined
                           && Func_attr[real_name].para_table[para_num-1].second==CHARTK
                           && cur_exp_type != 2){
                    printf("Line %d : 当前参数应为char类型而传进来了整型???\n", curLine);
                    error(para_type_wrong);
                } else {
                    printf("Line %d : 该函数名没有被定义 或 类型匹配 cur_exp_type = %d \n", curLine, cur_exp_type);
                    // 这里不报错啊！！！
                }
            } // 如果值参数表的个数已经比规定的要多，只进行语法分析，不用再比较函数的标准参数表了
        }
    }
    if (func_is_defined) { // 定义过函数再查
        if (para_num != Func_attr[real_name].para_name.size()) { // 参数个数不匹配
            cout << "Line : " << curLine << " using function " << Func_attr[real_name].Func_name
            << " value para number not match" << ", para_num is " << para_num
            << ", para_table_size " << Func_attr[real_name].para_name.size() << endl;
            error(para_num_wrong);
        } //   报错后正常继续
    }
    // 若sym 一进来就是 ')' 为空 什么都不用判断
    buffer[buf_pointer++] = "<值参数表>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
}

string express() {    // <表达式> 注意：2*('a'); 'a'*'b'这种都算 int 类型表达式
    // <表达式> ::= ［＋｜－］<项>{<加法运算符><项>}   //[+|-]只作用于第一个<项>
    // 减法运算符 MINU 加法运算符 PLUS 否则开头是 <标识符号、>
    string ret_exp = "";
    vector<string> args;
    queue<string> ops; // 不同项加减之间的连接符，这应该是个队列
    cur_exp_type = 0; // expression type = 0 的意思是空 或表达式里面有标识符 未定义
    string head_op = "";
    if(sym == PLUS || sym == MINU) {
        head_op = (sym == PLUS)? "+" : "-";
        sym = getSym();
    }
    // 注意，第一项可能带负号
    if (head_op == "-") { // 第一项变成 0 -
        args.push_back(to_string(0));
        ops.push("-");
    }
    string arg1 = item(); // 不管是什么，肯定有 项 ，可能现在进来 sym 就是标识符，也有可能是一个 整数，'(',
    args.push_back(arg1);
    while (sym == PLUS|| sym == MINU) {
        cout << sym << endl;
        if (sym == PLUS) {
            ops.push("+");
        } else {
            ops.push("-");
        }
        sym = getSym(); // 读到因子的开始
        arg1 = item();         // 递归下降
        args.push_back(arg1);  // 最后倒序相加
        cur_exp_type = 1; // 一旦有加减计算，即使用char 来计算，表达式也是整型的
    }
    // 这里应该是从左到右加！
    // t1 = arg[0] + arg[1] 中间代码
    int needMid = 0;
    int tmpInteger = 0;     // 用于常数优化
    
    if (args.size() >= 2) {
        if (isInteger(args[0]) && isInteger(args[1])) { // 常数计算，不输出中间代码
            int tmp0 = stoi(args[0]) , tmp1 = stoi(args[1]);
            if (ops.front() == "+") {
                tmpInteger = tmp0 + tmp1;
            } else if (ops.front() == "-") {
                tmpInteger = tmp0 - tmp1;
            } else if (ops.front() == "*") {
                tmpInteger = tmp0 * tmp1;
            } else if (ops.front() == "/") {
                tmpInteger = tmp0 / tmp1;
            }
            ops.pop(); // 弹出一个符号
        } else {
            needMid++;
            midCode mid(1, genTempReg(), args[0], ops.front(), args[1]);
            Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
            ops.pop(); // 弹出
            outputMid(mid);
        }
        
        for (int i = 2; i < args.size(); i++) {
            if (needMid == 0) {
                if (isInteger(args[i])) { // 下一个要计算的还是常数
                    int tmpi = stoi(args[i]);
                    if (ops.front() == "+") {
                        tmpInteger = tmpInteger + tmpi;
                    } else if (ops.front() == "-") {
                        tmpInteger = tmpInteger - tmpi;
                    } else if (ops.front() == "*") {
                        tmpInteger = tmpInteger * tmpi;
                    } else if (ops.front() == "/") {
                        tmpInteger = tmpInteger / tmpi;
                    }  // 不用输出中间代码
                } else {    // 第一次遇见下一个计算的不是常数
                    needMid++;
                    string integer = to_string(tmpInteger);
                    midCode mid(1, genTempReg(), integer, ops.front(), args[i]);
                    Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
                    outputMid(mid);
                }
                ops.pop();
            } else {
                needMid++;  // 后面应该也不会再用到了
                midCode mid(1, genTempReg(), getLastTempReg(), ops.front(), args[i]);
                Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
                ops.pop();
                outputMid(mid);
            }
        }
        
        if (needMid == 0) {
            ret_exp = to_string(tmpInteger);
        } else {
            ret_exp = getCurTempReg(); // 要返回的应该是当前最新的中间临时变量（寄存器值）
        }
    } else { // 没有后续的运算符，该表达式只有一个项，那直接返回这个项就行了
        ret_exp = args[0];
    }
    
    buffer[buf_pointer++] = "<表达式>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    return ret_exp;
}
string item() {  // <项>
    // <项>  ::= <因子>{<乘法运算符><因子>}
    // 乘法运算符 MULT
    string ret_item = "";
    vector<string> args;
    queue<string> ops; // 不同项加减之间的连接符
    string arg1 = factor();
    args.push_back(arg1);
    while (sym == MULT || sym == DIV) {
        cout << sym << endl;
        if (sym == MULT) {
            ops.push("*");
        } else {
            ops.push("/");
        }
        sym = getSym();
        arg1 = factor(); // 它会预读
        args.push_back(arg1);
        cur_exp_type = 1;
    }
    // 如果没有进入 while 循环，返回的字符串就是 factor 返回的东西, 可能是单个数字，名字，或者 #tx
    int needMid = 0;
    int tmpInteger = 0;     // 用于常数优化
    if (args.size() >= 2) {
        if (isInteger(args[0]) && isInteger(args[1])) { // 常数计算，不输出中间代码
            int tmp0 = stoi(args[0]) , tmp1 = stoi(args[1]);
            if (ops.front() == "+") {
                tmpInteger = tmp0 + tmp1;
            } else if (ops.front() == "-") {
                tmpInteger = tmp0 - tmp1;
            } else if (ops.front() == "*") {
                tmpInteger = tmp0 * tmp1;
            } else if (ops.front() == "/") {
                tmpInteger = tmp0 / tmp1;
            }
            ops.pop(); // 弹出一个符号
        } else {
            needMid++;
            midCode mid(1, genTempReg(), args[0], ops.front(), args[1]);
            Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
            ops.pop(); // 弹出
            outputMid(mid);
        }
        // t2 = args[n-3] + t1 ..., t3 = args[n-4] + t2
        for (int i = 2; i < args.size(); i++) {
            if (needMid == 0) {
                if (isInteger(args[i])) { // 下一个要计算的还是常数
                    int tmpi = stoi(args[i]);
                    if (ops.front() == "+") {
                        tmpInteger = tmpInteger + tmpi;
                    } else if (ops.front() == "-") {
                        tmpInteger = tmpInteger - tmpi;
                    } else if (ops.front() == "*") {
                        tmpInteger = tmpInteger * tmpi;
                    } else if (ops.front() == "/") {
                        tmpInteger = tmpInteger / tmpi;
                    }
                } else {    // 第一次遇到下一个计算的不是 integer 而是变量的名字了，又不能用lastTempReg
                    needMid++;
                    string integer = to_string(tmpInteger);
                    midCode mid(1, genTempReg(), integer, ops.front(), args[i]);
                    Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
                    outputMid(mid);
                }
                ops.pop();  // 最后弹出一个符号
            } else {
                needMid++;
                midCode mid(1, genTempReg(), getLastTempReg(), ops.front(), args[i]);
                Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
                ops.pop();
                outputMid(mid);
            }
        }
        if (needMid == 0) {
            ret_item = to_string(tmpInteger);   // 从头到尾都只有常数计算
        } else {
            ret_item = getCurTempReg(); // 要返回的应该是当前最新的中间临时变量（寄存器值）
        }
    }
    else { // 没有后续的运算符，该项只有一个因子
        ret_item = args[0];
    }

    buffer[buf_pointer++] = "<项>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    return ret_item; // 应该是当前最新临时变量的值
}
string factor() {  // <因子>, **这一层决定这个表达式到底是 int 还是 char 类型**
    // <因子>    ::= <标识符>｜<标识符>'['<表达式>']'
    //               |'('<表达式>')'
    //              ｜<整数>|<字符>｜<有返回值函数调用语句>
    // 变量名和数组的某个元素 <标识符> 前缀重复了，<有返回值函数调用语句> 开头也是标识符 注意一下
    string ret_factor = "";  // 如果是 <整数>或者<字符> 或者单个 <标识符>返回它自己，否则返回临时寄存器 #tx
    if (sym == IDENFR) { // 有可能是 <有返回值函数调用语句> name(
        string tmp_idenfr = idenfr; // 标记因子的标识符号:数组名/函数名
        cout << idenfr << endl;
        sym = getSym();
        if (sym == LPARENT) { // 读到 '(' 发现是函数调用, 此时可以判断函数返回的类型
            ret_factor = call_stat(); // 注意！里面会改变 idenfer!!
            // 里面统一会检查函数未定义，所以外面不用重复报错, 本次生成中间代码不考虑函数调用
            cout << "函数调用后, 此时 cur_exp_type =" << cur_exp_type << endl;
            // 这里判断函数调用语句相当于一个因子，不要因为 call_stat 而改变了表达式的值
            // 仅仅存在一个因子的时候，表达式返回值是什么类型就是什么类型，所以判断要放在最后
            if (Func_attr.count(tmp_idenfr) != 0) { // 这里只能用 tmp_idenfr!!!
                if (Func_attr[tmp_idenfr].getType() == 1) {
                    cur_exp_type = 1;
                } else if (Func_attr[tmp_idenfr].getType() == 2) { // 如果仅有函数返回 char 表达式也是 char
                    cur_exp_type = 2;
                    cout << "表达式的因子出现char函数调用, 此时 cur_exp_type == 2" << endl;
                } // 决定完毕
            }
        }
        else if (sym == LBRACK){    // 读到这里发现 是标识符[<表达式>] 数组下标,此时应该先check一下定义了没有
            //cout << '[' << " ready into express again! "<< endl;
            sym = getSym();
            string tmp_exp = express(); // 递归下降
            if (cur_exp_type != 1) { // cur_exp_type说的是内层的表达式若不是整型报错
                printf("Line %d : 数组下标表达式不是整型的\n", curLine);
                error(array_index_int);
            }
            // 读完下标，生成中间代码#tx = arr[?] , <type = 2, t1, arr_name[tmp_exp]>
            string arr_elm = tmp_idenfr + "[" + tmp_exp + "]";
            // 注意数组元素是不是
            midCode mid(2, genTempReg(), arr_elm);
            Func_attr[curFunc].allocVarAddr(getCurTempReg(), 0);
            outputMid(mid);
            
            if (sym == RBRACK) {  // ']'
                //cout << ']' << "end of express in 下标"<< endl;
                sym = getSym(); // 然后查找当前数组元素属于 char 还是int
            } else {
                printf("Line %d : [<表达式>]后面缺少']'\n", curLine);
                error(should_have_rbrack);
            }
            ret_factor = getCurTempReg();
            // 修改当前表达式类型是该数组元素类型
            cur_exp_type = checkIdenfrDef("NULL", tmp_idenfr, true);
            // 非数组元素内部会查未定义，若没定义返回0
        }
        else { // 判断刚才那个单个标识符
            // 单个标识符不需要用临时寄存器存下来
            ret_factor = tmp_idenfr;
            cur_exp_type = checkIdenfrDef("NULL", tmp_idenfr, true); // 返回1或2或0
        }
    } else if (sym == LPARENT) {  // '('<表达式>')'
        //cout << '(' << "ready into express again! "<< endl;
        cur_exp_type = 1; // 一旦出现括号括起来就是 int
        sym = getSym();
        string tmp = express(); // 递归下降，表达式会返回一个临时寄存器,它也会生成好中间代码
        // 这里只需要把返回的因子 赋上express 返回的临时变量就行
        ret_factor = tmp;
        if (sym == RPARENT) {
            //cout << ')' << "end of express 括号中的表达式"<< endl;
            cur_exp_type = 1;
            sym = getSym();
        } else {
            printf("Line %d : (表达式) 后缺少 ')'\n", curLine);
            error(should_have_rparent);
        }
    }
    else if (sym == PLUS || sym == MINU || sym == INTCON) { // 整数
        cout << sym << endl;
        cur_exp_type = 1;
        if(!integer()) {
            printf("Line %d : 不是整数 不符合词法\n", curLine);
            error(unvalid_char);
        }
        ret_factor = cur_integer; // 因子是一个整数字符串
    } else if (sym == CHARCON) {
        // <字符> ::=  '<加法运算符>'｜'<乘法运算符>'｜'<字母>'｜'<数字>'
        cout << cur_char_con << endl;
        cur_exp_type = 2;
        string char_con = " ";
        char_con[0] = cur_char_con;
        ret_factor = "'" + char_con + "'";  // 'a'
        sym = getSym();
    }
    buffer[buf_pointer++] = "<因子>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    return ret_factor;
}
void assign_stat() { // <赋值语句>
    // <赋值语句> ::=  <标识符>＝<表达式>|<标识符>'['<表达式>']'=<表达式>
    // 从 '=' 开始的, 进来之前已经读过前面的标识符了，可以直接调用 idenfr
    // 查找 这个 idenfr 是不是 全局常量或者当前函数的 常量
    arr_be_assigned = false;
    string assignTo_idenfr = idenfr;
    if (global_cons.count(idenfr) == 1) {
        cout << "Line : " << curLine << "不能对全局常量赋值 " << idenfr << endl;
        error(cons_not_be_changed);
    } else if (Func_attr[curFunc].cons_table.count(idenfr) == 1) {
        cout << "Line : " << curLine << "不能对当前函数的常量赋值 " << idenfr << endl;
        error(cons_not_be_changed);
    }
    if (sym == ASSIGN) {
        arr_be_assigned = false;
        cout << "=" << endl;
        sym = getSym();
        string result = express(); // 返回的可能是一个函数调用
        midCode mid(2, assignTo_idenfr, result);
        cout << "assign to idendr " << assignTo_idenfr <<  " = " << result << endl;
        outputMid(mid);
    } else if (sym == LBRACK) { // '[', 数组元素被赋值，检查类型
        arr_be_assigned = true; // 影响后面生成的中间代码 运算符种类 '[' ,']'
        sym = getSym();
        string index = express();
        if (cur_exp_type != 1) {
            printf("Line %d : 数组下标元素表达式不是整型 cur_exp_type = %d\n", curLine, cur_exp_type);
            error(array_index_int);
        }
        if (sym == RBRACK) {
            sym = getSym();
        } else {
            printf("Line %d : 数组下标后面缺少']'\n", curLine);
            error(should_have_rbrack);
        }
        if (sym == ASSIGN) {
            arr_be_assigned = false;    // 等号前面部分已经结束，后面是赋值的表达式
            sym = getSym();
            string result = express();  // 它会预读
            assignTo_idenfr = assignTo_idenfr + "[" + index + "]";
            midCode mid(2, assignTo_idenfr, result);
            outputMid(mid);
        }
    }
    buffer[buf_pointer++] = "<赋值语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    
}
void read_stat() {  // <读语句>
    // <读语句> ::=  scanf '('<标识符>{,<标识符>}')'
    // 其实可以不用判断scanf
    // 生成代码，在这里计数，看看要scanf进去多少个标识符
    sym = getSym();
    if (sym == LPARENT) {
        sym = getSym();
        if (sym == IDENFR) {
            int type = checkIdenfrDef("NULL", idenfr, true);
            string int_or_char = (type == 1)? "int" : "char";
            midCode mid(7, "scanf", int_or_char , idenfr);
            outputMid(mid);
            sym = getSym();
            while(sym == COMMA) {  // 可能有很多个标识符
                sym = getSym();
                if (sym == IDENFR) {
                    type = checkIdenfrDef("NULL", idenfr, true);
                    int_or_char = (type == 1)? "int" : "char";
                    midCode mid1(7, "scanf", int_or_char , idenfr);
                    outputMid(mid1);
                    sym = getSym(); // 再读下一个 ,
                }
            }
            if (sym == RPARENT) {
                sym = getSym();     //  预读
            } else {
                printf("Line %d : scanf语句后缺少 ')'\n", curLine);
                error(should_have_rparent); //
            }
        }
    }
    buffer[buf_pointer++] = "<读语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    
}
void write_stat() {     // <写语句>
    // <写语句> ::= printf '(' <字符串>,<表达式> ')'
    //              | printf '('<字符串> ')'| printf '('<表达式>')'
    string str = "";
    string exp = "";
    sym = getSym();
    if (sym == LPARENT) {
        sym = getSym();
        if (sym == STRCON) {
            str = cur_str_con;
            isString();
            if (sym == COMMA) {
                sym = getSym();
                exp = express(); // exp有可能是空的
                if (sym == RPARENT) { // ')'
                    sym = getSym();
                } else {
                    printf("Line %d : printf 语句缺少 ')'\n", curLine);
                    error(should_have_rparent);
                }
            } else if (sym == RPARENT) {
                sym = getSym();
            } else {
                printf("Line %d : printf 语句缺少 ')'\n", curLine);
                error(should_have_rparent);
            }
        } else {   // '(' 后面不是字符串，直接检测有没有 <表达式>
            exp = express(); // 里面会更新表达式类型
            if (sym == RPARENT) { // ')'
                sym = getSym();
            } else {
                printf("Line %d : printf 语句缺少 ')'\n", curLine);
                error(should_have_rparent);
            }
        }
    } // type = 6
    string StrName;
    int exp_type = 0;
    if (exp != "") {
        exp_type = cur_exp_type;    // 直接get最新表达式类型
        cout << "这个表达式不为空 检查表达式类型" << exp << " " << exp_type << endl;
    }
    if (exp[0] == '\'' || exp_type == 2) { // 标识符也有可能是 char ch, arr[0]
        // 是一个char 类型的
        if (str == "") {
            midCode mid(6, "print", "", "char", exp);
            outputMid(mid);
        } else {
            StrName = genStrName();
            midCode mid(6, "print", StrName, "char", exp);
            outputMid(mid);
        }
    } else { // exp是 int 类型，第三个参数i写 NULL
        if (str == "") {
            midCode mid(6, "print", "", "NULL", exp);
            outputMid(mid);
        } else {
            StrName = genStrName();
            midCode mid(6, "print", StrName, "NULL", exp);
            outputMid(mid);
        }
    }
    if (str != "") { // 整个程序只有这个地方用到了<字符串>常量, 下面建立 "str1" 和字符串内容的映射
        strContainer.insert(make_pair(StrName, str));
    }
    
    buffer[buf_pointer++] = "<写语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);

}
void isString() {
    // 直接用 sym == STRCON 即可
    buffer[buf_pointer++] = "<字符串>";
    sym = getSym(); // 后面是逗号 , ')'
}
void return_stat() {    // <返回语句>
    // <返回语句> ::=  return['('<表达式>')']
    //cout << "进入 return 语句检测 curFunc = "<< curFunc << endl;
    sym = getSym();
    string ret = "";
    if (sym == LPARENT) {
        sym = getSym();
        ret = express();
        if (cur_exp_type != 0) { // 表达式不为空
            have_return_stat = true; // 1 2说明表达式类型是 int 或 char
            //cout << "return 语句表达式 = "<< cur_exp_type << endl;
        }
        if (sym == RPARENT) {
            if (Func_attr[curFunc].getType() != cur_exp_type
                && cur_exp_type!=0 && Func_attr[curFunc].getType()!=0) {
                printf("Line %d : return语句返回类型与函数返回类型不匹配\n",curLine);
                error(missing_return);
            }
            if (Func_attr[curFunc].getType() == 0 && have_return_stat) {
                printf("Line %d : 没有返回值的函数多了 return 语句\n", curLine);
                error(return_stat_occur);
            }
            sym = getSym();
        } else {
            printf("Line %d : return 语句缺少 ')' 后续不进行函数返回值类型检查\n", curLine);
            error(should_have_rparent);
        }
        // 错误处理完毕
    }
    buffer[buf_pointer++] = "<返回语句>";
    swap(buffer[buf_pointer-1], buffer[buf_pointer-2]);
    midCode mid(2, "ret", ret); // 只有一句 return; 的时候 ret == ""
    outputMid(mid);
}

string genStrName(){
    string strName = "";
    strNum++;
    strName = "str" + to_string(strNum);
    //cout << "gen str name " << strName << endl;
    return strName;
}

void genMips(){
    outmips.open(MIPSPATH, ios::trunc); // 以写模式打开文件
    outmips << ".data" << endl; // 数据段
    // 先生成一个仅包含换行符的字符串 str0
    
    for (int i = 0; i < global_vars_mips_data.size(); i++) {
        cout << "全局变量 .data 定义 " << endl;
        outmips << global_vars_mips_data[i] << endl;
    }
   
    outmips << "enter: .asciiz \"\\n\"" << endl; // 换行时调用这个名字
    string str_tmp_name;
    for (int i = 1; i <= strNum; i++) {
        str_tmp_name = "str" + to_string(i);
        outmips << str_tmp_name << ": .asciiz "; // 输出字符串代号名字
        outmips << "\"" << strContainer[str_tmp_name] << "\"" << endl;
    }
    
    outmips << ".text" << endl; // 代码段
    
    outmips << "j main_" << endl; // 一开始先进入main函数
    int size = midCodeSize();
    int p = 0; // 开始一条一条读中间代码
    
    bool inForLoop = false;
    while (p < size) {
        map<string, int> int_cons_value = Func_attr[curFunc].int_cons_value;
        map<string, char> char_cons_value = Func_attr[curFunc].char_cons_value;
        map<string, int> vars_addr = Func_attr[curFunc].vars_addr;
        Func_attribute func_attr = Func_attr[curFunc];
        // 生成mips使用的是临时的 map ，因为cons值固定，不需要再修改，而vars_addr也是固定的
        midCode code = getNextMidCode(p);
        string op = code.op;
        string arg1 = code.arg1;
        string arg2 = code.arg2;
        string result = code.result; // #t1, n[i], 其他关键字
        if (code.type == 1) { // 标准表达式赋值，四元式, result
            if(result[0] == '#' || isalpha(result[0])) { // #t1 , a + 1, i=i+1 这里不会出现数组形式的东西
                int result_addr = vars_addr[result]; // 取出临时给(中间)变量分配的地址
                bool shouldGoMem = false;
                string reg3; // 用来存结果
                if (result[0] == '#') {
                    reg3 = getReg(result);  // lock <#s1, 2> 意为 #s2 与 2号寄存器绑定，后面无需 sw
                }
                else if (concludeVarMap.count(result) == 1 && func_attr.findArgumentPos(result) == 0) {
                    reg3 = concludeVarMap[result];  // 归纳变量
                }
                else {
                    shouldGoMem = true;
                    reg3 = getReg();
                }
                string reg1 = getReg(), reg2 = getReg();
                bool reg1_isInt = false, reg2_isInt = false;
                int int1 = 0, int2 = 0;
                if (isInteger(arg1) && !isInteger(arg2) && op == "+") {
                    reg1_isInt = true;
                    int1 = stoi(arg1);
                    reg2 = load_argument(arg2, reg2);
                } else if (isInteger(arg2) && !isInteger(arg1) && op == "+") { // 大多数是这种情况
                    reg2_isInt = true;
                    int2 = stoi(arg2);
                    reg1 = load_argument(arg1, reg1);
                } else if (isInteger(arg2) && !isInteger(arg1) && op == "-") {
                    reg2_isInt = true;
                    int2 = stoi(arg2);
                    reg1 = load_argument(arg1, reg1);
                }
                else {
                    reg1 = load_argument(arg1, reg1); // 如果arg1是中间变量存过，那么返回已经存有值的那个寄存器，
                    reg2 = load_argument(arg2, reg2);
                }
                if (op == "+") { // add $t1, $t2, $t3; sw $t3, -400($sp)
                    if (reg1_isInt) { // xxx = 2 + xxx
                        outmips << "addi " << reg3 << "," << reg2 << ", " << int1 << endl;
                    } else if (reg2_isInt) {
                        outmips << "addi " << reg3 << "," << reg1 << ", " << int2 << endl;
                    } else {
                        outmips << "add " << reg3 << "," << reg1 << ", " << reg2 << endl;
                    }
                }
                else if (op == "-") {
                    if (reg2_isInt) { // i = i - 1, addi reg3, reg1, -int2,  加后面常数的相反数
                        outmips << "addi " << reg3 << "," << reg1 << ", " << -int2 << endl;
                    } else {
                        outmips << "sub " << reg3 << "," << reg1 << ", " << reg2 << endl;
                    }
                }
                else if (op == "*") { // m = n * 2, n*4, n*8, 4*n
                    if (isInteger(arg2) && !isInteger(arg1) && isPowerOf2(arg2) != 0) {
                        outmips << "sll " << reg3 << "," << reg1 << ", " << isPowerOf2(arg2) << endl;
                    } else if (isInteger(arg1) && !isInteger(arg2) && isPowerOf2(arg1) != 0) {
                        outmips << "sll " << reg3 << "," << reg2 << ", " << isPowerOf2(arg1) << endl;
                    } else {
                        outmips << "mul " << reg3 << "," << reg1 << ", " << reg2 << endl;
                    }
                    
                }
                else if (op == "/") { // xxx = x / 2
                    if (isInteger(arg2) && !isInteger(arg1) && isPowerOf2(arg2) != 0) {
                        outmips << "srl " << reg3 << "," << reg1 << ", " << isPowerOf2(arg2) << endl;
                    } else {
                        outmips << "div " << reg1 << ", " << reg2 << endl;
                        outmips << "mflo " << reg3 << endl;
                    }
                }
                
                if (shouldGoMem) {
                    outmips << "sw " << reg3 << ", " << result_addr << "($sp)" << endl;
                }
            } else {
                cout << "类型为1的 result 不是#tx ,  报错" << endl;
            }
        }
        else if (code.type == 2) { // 二元，赋值中含有数组元素，最终结果赋值
            if (result == "goto") { // 跳转到的标签先去掉末尾的冒号 size()-1
                outmips << "j " << arg1.substr(0, arg1.size()-1) << endl; // jump unconditionally label
            }
            else if (result == "int" || result == "char" || result == "void"){ // 函数头
                string call_func_name = arg1;
                curFunc = call_func_name; // 读中间代码的时候仅在这里更新 curFunc
                outmips << call_func_name << "_:" << endl;
                // 进到这个函数的时候，先把 sp 向下移动整个函数的 offset
                // 第一个4位已经预留给 $ra
                int func_sp_offset = Func_attr[call_func_name].getFuncSpOffset();
                
                outmips << "addi $sp, $sp, -" << func_sp_offset+52 << endl;
                // 向下移动这个函数的符号表大小，其中52位是用来存到栈的
                // 0($sp)是上一一个函数内 jal 的时候 mips 自动存到了$ra里
                outmips << "sw $ra, 0($sp)" << endl;
            }
            else if (result == "push") { // 调用函数前，压栈传参，并不知道调用的是那个，所以只能先
                // arg1 是当前函数的（中间）变量名？局部常量名/全局变量名/全局常量名/整数/字符
                push_vector.push_back(arg1);
                // 如果这是全局变量，后面的 push 如果是函数调用可能会导致前面参数的变量名代表的数改变
            }
            else if (result == "call") { // jal function_name, 要判断一下函数是不是 void
                storeA1();
                
                string target_function = arg1; // 目标函数名
                map<string, int> target_vars_addr = Func_attr[target_function].vars_addr;
                vector<pair<string, string>> para_table = Func_attr[target_function].para_table;
                int tar_sp_offset = Func_attr[target_function].getFuncSpOffset();
                string target_sp = getReg();
                
                outmips << "addi " << target_sp << ", $sp , -" << tar_sp_offset+52 << endl;
                // 此时 target_sp 存着新移动后的 $sp ，（$sp还未真正移动）
                
                // 每次 $sp 移动的时候是要多移动 52 留给 $s0-$s7 和临时 t 来存的
                int push_size = push_vector.size();
                for (int i = 0; i < para_table.size(); i++) { // para_table 和 push vector 应该一样大
                    string load_para_reg = getReg();
                    load_para_reg = load_argument(push_vector[push_size-para_table.size()+i], load_para_reg);
                    
                    string para_name = para_table[i].first; // 第 i 个参数的名字
                    int para_offset = target_vars_addr[para_name]; // 获得第i个参数相对于它自己函数的偏移量
                    //outmips << "TIPS: storing argument into function corresponding addr " << endl;
                    outmips << "sw " << load_para_reg << ", " << para_offset << "(" << target_sp<< ")" <<  endl;
                    
                }// 装载参数完毕,记得清空此次调用的push_vector!!
                // 只能清空倒数 para_table 个！不能全都清空
                for (int i = 0; i < para_table.size(); i++) {
                    push_vector.pop_back(); // 这玩意儿做成一个栈会好一点
                }
                // sp 移动目标函数偏移 放到 函数头去完成
                outmips << "jal " << target_function << "_" << endl;
                // 回来的时候 sp 已经移动好了, 恢复 a1-a3寄存器
                restoreA1();
            }
            else if (result == "ret") { // ret res , ret #t4 (说明返回语句是return(<表达式>) 这个样子)
                //读到第一次生成中间代码 ret 的时候实际上还没有读到过 call
                // 把右侧返回值(arg1) load 到 return_reg 里面
                string return_reg = getReg();
                if (arg1 != "") {
                    return_reg = load_argument(arg1, return_reg);
                    outmips << "move $v1, " << return_reg << endl;
                }
                // 然后,取出 4($sp) 得到 $ra, 取出 0($sp)的得到应该恢复的 sp, 然后 $jr ra
                outmips << "lw $ra, 0($sp)" << endl;
                
                if (curFunc != "main") {
                    //最后移动sp, 注意，如果是main函数有return;语句 不要再跳了！
                    int restore_sp = Func_attr[curFunc].getFuncSpOffset();
                    outmips << "addi $sp, $sp, " << restore_sp << endl; // 恢复（上移）$sp
                    outmips << "jr $ra" << endl;
                } else { // main 函数中任何一个地方有return
                    outmips << "li $v0, 10" << endl;
                    outmips << "syscall" << endl;
                }
            }
            else { // y = ret
                two_elem_assign(result, arg1);
            }
            
        }
        else if (code.type == 3) { // 三元，为假跳转，为真跳转
            cout << "按理不会跳到 type == 3的中间语句，前面必然是条件赋值" << endl;
        }
        else if (code.type == 4) { // 变量定义，常量定义，参数列表，暂不考虑
            // 要移动 真实的 $sp ???
            // 分配的工作都已经在生成中间代码的时候完成了，这里的mips好像不用生成什么东西
        }
        else if (code.type == 5) { // 条件的是非结果中间变量只是放到寄存器里，不会存到内存里
            // #t8 i GRE 0
            string condition_res = getReg(); // 用来存条件真假结果的寄存器
            // if arg1 == 1, reg = 1, else reg = 0
            // condition result 已经设置好
             p++; // 最后再p++，这样下一句就不会再跳一遍 code.type == 3的情况了
             midCode nextcode = getNextMidCode(p); // 肯定是跳转语句
             string label = nextcode.op.substr(0, nextcode.op.size()-1);
            
            if (op == "" && arg2 == "") { // 条件只有一个表达式的
                condition_res = load_argument(arg1, condition_res);
                if (nextcode.result == "BZ") { // 为假跳转, cond == 0
                    outmips << "beqz " << condition_res << ", " << label << endl;
                } else if (nextcode.result == "BNZ") { // 为真跳转 cond != 0
                    outmips << "bnez " << condition_res << ", " << label << endl;
                }
            }
            else { // 需要比较两个表达式, 先分别load好 tmp_reg1, tmp_reg2后续比较这两个寄存器
                string tmp_reg1 = getReg(), tmp_reg2 = getReg();
                tmp_reg1 = load_argument(arg1, tmp_reg1);
                tmp_reg2 = load_argument(arg2, tmp_reg2);
                outmips << "sub " << condition_res << ", " << tmp_reg1 << ", " << tmp_reg2 << endl;
                // 相减然后比较与 0 的关系
                if (op == LSS) { // a < b
                    if (nextcode.result == "BZ") { // 为假跳转, a >=b
                        outmips << "bgez " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 a < b
                        outmips << "bltz " << condition_res << ", " << label << endl;
                    }
                } else if (op == LEQ) { // a <= b
                    if (nextcode.result == "BZ") { // 为假跳转，a > b,
                        outmips << "bgtz " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 a <= b
                        outmips << "blez " << condition_res << ", " << label << endl;
                    }
                } else if (op == GRE) { // a > b ,
                    if (nextcode.result == "BZ") { // 为假跳转，a <= b
                        outmips << "blez " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 a > b
                        outmips << "bgtz " << condition_res << ", " << label << endl;
                    }
                } else if (op == GEQ) { //a >= b set greater or equal
                    if (nextcode.result == "BZ") { // 为假跳转，a < b
                        outmips << "bltz " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 a > =b
                        outmips << "bgez " << condition_res << ", " << label << endl;
                    }
                } else if (op == EQL) { // a == b, set equal
                    if (nextcode.result == "BZ") { // 为假跳转，a != b
                        outmips << "bnez " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 a == b
                        outmips << "beqz " << condition_res << ", " << label << endl;
                    }
                } else if (op == NEQ) { // a != b, set not equal
                    if (nextcode.result == "BZ") { //为假跳转，a ==b
                        outmips << "beqz " << condition_res << ", " << label << endl;
                    } else if (nextcode.result == "BNZ") { // 为真跳转 not equal to zero
                        outmips << "bnez " << condition_res << ", " << label << endl;
                    }
                }
            }
        }
        else if (code.type == 6) { // 四元式， printf 语句，
            //后面表达式一旦有函数调用或者数组都会转化成 #tx中间变量，所以不用考虑
            // result == "print", arg1字符串名字，
            if (result != "print") { cout << "MidCode type = 6, result should be\"print\" " << endl; }
            if (arg1 != "" ) { // 输出字符串
                outmips << "li $v0, 4" << endl;
                outmips << "la $a0, " << arg1 << endl;
                outmips << "syscall" << endl;
            }
            if (arg2 != "") { // 输出表达式：op 代表后面表达式是不是 "char", arg2是后面的表达式exp若为 "" 则不输出
                string reg2 = getReg();
                if (op == "char") { // 字符型, 否则是int,  op == "NULL"或别的东西
                    if (arg2[0] == '\'') { // 输出字符print character， $v0 == 11, $a0 <- ASCII码
                        char ch = arg2[1];
                        int ascii = ch;
                        outmips << "li $v0, 11" << endl;
                        outmips << "li $a0, " << ascii << endl;
                        outmips << "syscall" << endl;
                    }
                    else { // 字符，全局变量，全局常量
                        reg2 = load_argument(arg2, reg2);  // 往 reg2 里装载东西
                        outmips << "li $v0, 11" << endl; // 打印服务
                        
                        outmips << "move " << "$a0, " << reg2 << endl;
                        outmips << "syscall" << endl;
                    }
                }
                else { // 表达式是整型的, 后面应该是一个 标识符名字， 或者 中间变量 #tx, var_addr获得地址取值
                    reg2 = load_argument(arg2, reg2);
                    // reg2装载完毕，下面调用打印服务
                    outmips << "li $v0, 1" << endl;
                    outmips << "move " << "$a0, " << reg2 << endl;
                    outmips << "syscall" << endl;
                }
            }
            // 【注意】最后mips要输出一个换行符! ! !
            outmips << "li $v0, 4" << endl;
            outmips << "la $a0, enter" << endl;
            outmips << "syscall" << endl;
        }
        else if (code.type == 7) { // 二元式，scanf语句, op 不可能是中间变量
            // result == "scanf", arg1 == "int" / "char", op = idenfr
            int addr = 0;
            if (result != "scanf") { cout << "type == 7 result != scanf 生成中间代码出错" << endl; }
            // scanf 进来的一定是全局变量或者局部变量，且不可能是数组元素
            bool is_global_var = false;
            if (vars_addr.count(op) == 1) {
                addr = vars_addr[op]; // 相对于该函数的地址
            } else if (global_vars.count(op) == 1) {
                // 全局变量的 地址是相对于满栈顶的偏移
                is_global_var = true;
            } else {
                cout << "scanf语句中找不到要读的变量名字" << endl;
            }
            if (arg1 == "int") {
                outmips << "li $v0, 5" << endl; // read integer
                outmips << "syscall" << endl; // $v0 contains integer read
            } else if (arg1 == "char"){
                outmips << "li $v0, 12" << endl; // read char
                outmips << "syscall" << endl; // $v0 contains character read
            } else {
                cout << "scanf 类型错误, 中间代码装进来的 不是 int | char " << endl;
            }
            if (!is_global_var) {
                // 是局部变量
                // 先考虑会不会是归纳变量，且不能是参数
                if (concludeVarMap.count(op) == 1 && func_attr.findArgumentPos(op) == 0) {
                    string conclude_reg = concludeVarMap[op];
                    outmips << "move " << conclude_reg << ", $v0" << endl;
                } else {
                    outmips << "sw $v0, " << addr << "($sp)" << endl;
                }
            }
            else { // 局部变量，相对于($sp)的地址, $sp就是 cur_real_sp的值
                outmips << "sw $v0, " << op << endl; // 用全局变量名当地址
            }
        }
        else if (code.type == 8) { // 一元，标签，各种labels
            outmips << result << endl;
            if (result[0] == 'f') { // 进入 for 循环
                if (inForLoop) {
                    cout << "for 循环的结束 " << endl;
                    inForLoop = false;
                } else {
                    cout << "发现了 for 循环的开始 " << endl;
                    inForLoop = true;
                }
            }
        }
        // 看下一句是不是一个新的函数定义而本句不是"ret", 即无返回值函数的最后一句
        if (p < size - 1) { // 注意不要越界
            midCode nextmidCode = getNextMidCode(p+1);
            string nresult = nextmidCode.result;
            if (nextmidCode.type == 2) {
                if (nresult == "int" || nresult == "char" || nresult == "void") { //下一句是新的函数定义
                    if (code.result != "ret" && code.type != 4) { // 本句不是返回语句,也不是各种声明
                        // 取出 0($sp) 得到 $ra
                        outmips << "lw $ra, 0($sp)" << endl;
                        int restore_sp = Func_attr[curFunc].getFuncSpOffset();
                        
                        // 最后再移动 sp !!!
                        outmips << "addi $sp, $sp, " << restore_sp << endl; // 恢复（上移）$sp
                        outmips << "jr $ra" << endl;
                    }
                }
            }
        }
        
        p++;
    }
    outmips.close();
}
string getReg(string lockMid){ // regNo, 这里只返回临时寄存器，只能用 $t0 ~ $t9
    string ret = "";
    int cnt = 0;  // 这里可能会导致死循环
    // 如果需要锁定的值，存在 $s0- s7中，一共8个
    if (lockMid != "") {    //
        while(saveRegisterGroup[sregNo] == 1) {  // 这个寄存器被占用了
            sregNo++;                        // 换下一个寄存器
            sregNo = sregNo % 8; // s0-s7
            cnt++;
        }
        ret = "$s" + to_string(sregNo); // $s
        lockedRegMap.insert(make_pair(lockMid, sregNo));
        saveRegisterGroup[sregNo] = 1;  // 锁定，在下一次用它的时候解锁
    } else { // t0-t7, t8, t9 扔一个没被占用的回去
        while (tmpRegisterGroup[tregNo] == 1) {
            tregNo++;
            tregNo = tregNo % 10;
        }
        // 得到一个没被占用的 $t 寄存器
        ret = "$t" + to_string(tregNo);
    }
    sregNo++;
    sregNo = sregNo % 8; // s0-s7
    
    tregNo++;
    tregNo = tregNo % 10; //t0-t9
    return ret;
}

void two_elem_assign(string result, string arg1) {
    map<string, int> int_cons_value = Func_attr[curFunc].int_cons_value;
    map<string, char> char_cons_value = Func_attr[curFunc].char_cons_value;
    map<string, int> vars_addr = Func_attr[curFunc].vars_addr;
    Func_attribute func_attr = Func_attr[curFunc];
    int addr_res = 0; // result 有可能是数组
    bool index_is_mid = false;
    string res_offset, res_addr_reg; // res_addr_reg 后者用来存要赋给 result 的真实地址
    bool result_is_global = false, result_is_arr_elem = false;
    string result_index = "", result_arr = "";
    if (vars_addr.count(result) == 1) { // 局部单个变量（含中间变量, 参数）
        addr_res = vars_addr[result]; // 如果是不经过内存的中间变量/参数，这句话其实用不到
    }
    else if (global_vars.count(result) == 1) { // 找全局变量
        result_is_global = true; // 全局单个变量
    } // 这里不会是常量
    else if (result.find("[") > 0) { // arr[1]有可能是数组，vars_addr里面有数组名和对应的起始地址，有可能是全局数组
        result_is_arr_elem = true; // 是数组
        string arr_name = result.substr(0, result.find("[")); // 数组名有可能是局部的或全局的
        result_arr = arr_name; // 左侧数组名记下来
        string index = result.substr(1+result.find("[")); // 掐头
        index = index.substr(0, index.size()-1); // 去尾 ，index可能是  #tx
        if (vars_addr.count(index) == 1) { // index是一个(中间)变量, 那么(中间)变量必然是局部的，直接vars_addr即可
            index_is_mid = true; // 是数组且下标index是(中间变量名)
            int mid_var_addr = vars_addr[index]; // 下标变量地址
            res_offset = getReg(); // 临时存，*4 后的结果
            string mul_4 = getReg(); // 准备要在上面乘4
            if (lockedRegMap.count(index) == 1) { // 中间变量在下标这里被用到，需要释放
                int sregNo = lockedRegMap[index];
                mul_4 = "$s" + to_string(sregNo);
                lockedRegMap.erase(index);
                saveRegisterGroup[sregNo] = 0;
            }
            else if (concludeVarMap.count(index) == 1 && func_attr.findArgumentPos(index) == 0) {
                mul_4 = concludeVarMap[index];  // 直接从表里取出key对应的value就是寄存器名
            }
            else {
                outmips << "lw " << mul_4 <<", "<< mid_var_addr << "($sp)" << endl; // 中间变量数>0
            }
            
            // 下标如果是用某个寄存器取出来不能改动
            outmips << "sll " << res_offset  << ", " << mul_4 << ", 2" << endl; // *4 = 左移两位
            res_addr_reg = getReg(); // 先存数组头地址
            // 应该先判断是不是局部的！！！
            if (vars_addr.count(arr_name) == 1){ // 左侧是局部数组， index 是(中间)局部变量 #t1， i， t
                addr_res = vars_addr[arr_name]; // 局部数组首地址, 相对本函数的地址
                outmips << "li " << res_addr_reg << ", " << addr_res << endl; // 取数组头地址
                outmips << "add " << res_addr_reg << ", " << res_addr_reg << ", " << res_offset << endl;
                outmips << "add " << res_addr_reg << ", " << res_addr_reg << ", " << "$sp" << endl;
            } // 现在得到局部数组 真正栈中地址，存在res_addr_reg中
            else if (global_vars.count(arr_name) == 1) { // 左侧是全局数组， index 是(中间)变量 #t1
                result_is_global = true; // result 代表着 .data里的一个label
                // 最后计算的时候 直接用标签 result_arr
            }
        }
        else if (int_cons_value.count(index) == 1) { // index 是局部常量名
            // index_is_mid = true;
            if (vars_addr.count(arr_name) == 1) {
                cout << "找到该局部数组 " << arr_name << " addd_res = "<< addr_res <<endl;
                addr_res = vars_addr[arr_name]; // 数组首地址
                addr_res = addr_res + 4*int_cons_value[index];  // 取出偏移大小int_cons_value[index]再乘4
                cout << arr_name << " addd_res = "<< addr_res <<endl;
            }
            else if (global_vars.count(arr_name) == 1) {
                cout << "左侧是全局数组 下标是全局 int 常量名称" <<endl;
                result_is_global = true; // 要弄出 res_offset 给后面用
                res_offset = getReg(); // 临时存, 偏移量 4*index
                outmips << "li " << res_offset << ", " << 4*int_cons_value[index] << endl;
            }
            else {
                cout << "左侧找不到该数组 " << arr_name << endl;
            }
            
        }
        else if (global_int_cons_value.count(index) == 1){ // index是全局int 常量名
            //index_is_mid = true;
            if (vars_addr.count(arr_name) == 1) { // 先找局部数组
                cout << "找到该局部数组 " << arr_name << " addd_res = "<< addr_res <<endl;
                addr_res = vars_addr[arr_name]; // 数组首地址
                addr_res = addr_res + 4*global_int_cons_value[index];
                cout << arr_name << " addd_res = "<< addr_res <<endl;
            }
            else if (global_vars.count(arr_name) == 1) { // 然后再找全局数组
                cout << "左侧是全局数组 下标是全局 int 常量名称" <<endl;
                result_is_global = true;
                res_offset = getReg(); // 临时存, 偏移量 4*index
                outmips << "li " << res_offset << ", " << 4*global_int_cons_value[index] << endl;
            } else {
                cout << "左侧找不到该数组 " << arr_name << endl;
            }
           
        }
        else if (global_vars.count(index) == 1) { // 左侧(不知道什么)数组,
            index_is_mid = true;
            res_offset = getReg(); // 临时存，下标乘 4
            outmips << "lw " << res_offset << ", " << index << endl; // 取下标值，index是全局变量名
            outmips << "sll " << res_offset << ", " << res_offset << ", 2" << endl; // 左移两位
            res_addr_reg = getReg(); // 先存数组头地址
            if (vars_addr.count(arr_name) == 1) { // 看是否是局部数组
                // 找数组首地址，最后会加上 res_offset
                addr_res = vars_addr[arr_name]; // 局部数组首地址, 相对本函数的地址
                outmips << "li " << res_addr_reg << ", " << addr_res << endl; // 取数组头地址
                outmips << "add " << res_addr_reg << ", " << res_addr_reg << ", " << res_offset << endl;
                outmips << "add " << res_addr_reg << ", " << res_addr_reg << ", " << "$sp" << endl;
            }
            else if (global_vars.count(arr_name) == 1) { // 看左侧是全局数组
                result_is_global = true; // 后面会直接用已经算好的 res_offset寄存器
            } else {
                cout << "左侧找不到该数组 " << arr_name << endl;
            }
            
        }
        else { // 是数组，且下标index是整数
            cout << arr_name << " 赋值给数组下标 必然是整数！result_index = " << index << endl;
            if (vars_addr.count(arr_name) == 1){ // 局部数组 arr[2]
                addr_res = vars_addr[arr_name]; // 数组首地址
                addr_res = addr_res + 4*stoi(index); // 这里stoi很有可能出问题
            } // 最后输出的时候 如果是 global 直接用标签
            else if (global_vars.count(arr_name) == 1) {
                result_is_global = true; // result_arr已经赋值好了
                result_index = index; // 全局数组下标是整数，用于后面存
            }
        }
    }
    else {
        cout << "type = 2 ,mips赋值对象没找到地址" << endl;
    }
    
    string reg = getReg(); // 接下来在 reg 装好要存的值
    // load arg
    reg = load_argument(arg1, reg);
    // 现在 reg 里面存好了要赋的值，需要sw reg 到 result 代表的内存空间里
    
    if (result_is_global) { // 全局变量, 直接用 result_arr标签寻址
        if (result_is_arr_elem) { // 全局数组
            if (index_is_mid) { // 赋值给的result 的数组下标是一个中间变量或者 idenfr
                //outmips << "TIPS: result is global , arr_elem, index is mid " << endl;
                outmips << "sw " << reg << ", " << result_arr << "(" << res_offset << ")" << endl;
            } else { // 全局数组，下标 是整数,局部常量，全局常量 sw $reg, result_arr+index
                int real_int_index = 0;
                real_int_index = stoi(result_index); // 去掉了 try catch
                real_int_index = real_int_index*4;
                outmips << "sw " << reg << ", " << result_arr << "+" << to_string(real_int_index) << endl;
            }
        } else { // 全局普通单个变量, result 即为它的名字标签，直接取地址
            outmips << "sw " << reg << ", " << result << endl;
        }
    }
    else { // 局部
        if (result_is_arr_elem) { // 局部数组
            if (index_is_mid) { // 左侧是局部数组且下标 #t1或 变量的 idenfr 要在上面直接计算完，寄存器存地址
                //outmips << "TIPS: result is NOT global, is arr_elem, index is mid " << endl;
                outmips << "sw " << reg << ", (" << res_addr_reg << ")" << endl;
            } else { // 局部数组，下标 是整数, 局部常量，全局常量，直接找到相对地址
                outmips << "sw " << reg << ", " << addr_res << "($sp)" << endl;
            }
        }
        else { // 局部普通单个变量, 也要用到 addr_res， addr_res = vars_addr[result];
            // 如果是中间变量，存临时寄存器，不要sw
            if (result[0] == '#' && arg1 != "ret") { // 分配并锁定该寄存器
                string allocReg = getReg(result);   // 把 reg 值存到 allocReg 里
                outmips << "move " << allocReg << ", " << reg << endl;
            } else if (concludeVarMap.count(result) == 1 && func_attr.findArgumentPos(result) == 0) {
                outmips << "move " << concludeVarMap[result] << ", " << reg << endl;
                // 赋值给归纳变量
            }
            else {
                // cout << "赋值：这个中间变量还是要存到内存里 " << result << endl;
                outmips << "sw " << reg << ", " << addr_res << "($sp)" << endl;
            }   // 函数返回值还是要放到内存里面存取一下
            
        }
    }
}

string load_argument(string arg1, string reg, bool unlock) { // unlock 默认为true arg1是要被装的东西：整数，变量名，常量名，reg是要装的寄存器
    map<string, int> int_cons_value = Func_attr[curFunc].int_cons_value;
    map<string, char> char_cons_value = Func_attr[curFunc].char_cons_value;
    map<string, int> vars_addr = Func_attr[curFunc].vars_addr;
    Func_attribute func_attr = Func_attr[curFunc];
    
    // arg 是当前函数的（中间）变量名？局部常量名/局部数组元素/全局变量名/全局数组元素/全局常量名/整数/字符
    // 要把现在参数的值存到 reg 里
    int addr_arg = 0; // arg1 有可能是数组
    string arg_offset;

    // 优化：增加一条判断，这里的(中间)变量名字是不是与某个寄存器绑定在一起了，如果是，那么直接拿寄存器来用
    if (lockedRegMap.count(arg1) == 1) { // 直接取出装有 arg1 的寄存器号拼接成
        int sregNo = lockedRegMap[arg1];   // 取出来的是sregNo
        string regForRet = "$s" + to_string(sregNo);    // 解锁仅仅与 $s0 至 $s7有关
        // 解锁两步操作（有两侧函数调用时，不能马上解锁）
        if (unlock) {   // 默认都是 true
            lockedRegMap.erase(arg1);
            saveRegisterGroup[sregNo] = 0;
            cout << "释放 " << regForRet << ", 配对 " << arg1 << endl;
        }
        return regForRet;
    }
    
    // 后考虑是不是归纳变量
    if (concludeVarMap.count(arg1) == 1 && func_attr.findArgumentPos(arg1) == 0) {
        return concludeVarMap[arg1];    // 返回归纳变量的寄存器名
    }
    
    if (isInteger(arg1)) { // 要把一个整数赋值给
        int number = stoi(arg1);
        outmips << "li " << reg << ", " << number << endl;
    }
    else if (arg1[0] == '\'') { // 被赋值的是一个字符
        char ch = arg1[1];
        int ascii = ch;
        cout << "load argument is a char" << endl;
        outmips << "li " << reg << ", " << ascii << endl;
    }
    else if (vars_addr.count(arg1) == 1) {  //找到这个局部变量或中间变量的地址 #tx
        addr_arg = vars_addr[arg1];   // lw
        outmips << "lw " << reg << ", " << addr_arg << "($sp)" << endl;
    }
    else if (int_cons_value.count(arg1) == 1) { // 局部int常量
        outmips << "li " << reg << ", " << int_cons_value[arg1] << endl;
    }
    else if (char_cons_value.count(arg1) == 1) { // 局部 char 常量
        int ascii = char_cons_value[arg1];
        outmips << "li " << reg << ", " << ascii << endl;
    }
    else if (global_vars.count(arg1) == 1) { // 全局变量
        outmips << "lw " << reg << ", " << arg1 << endl; // 标签获取全局变量地址
    }
    else if (global_int_cons_value.count(arg1) == 1) { // 全局int常量
        outmips << "li " << reg << ", " << global_int_cons_value[arg1] << endl;
    }
    else if (global_char_cons_value.count(arg1) == 1) { // 全局 char 常量
        int ascii = global_char_cons_value[arg1];
        outmips << "li " << reg << ", " << ascii << endl;
    }
    else if (arg1 == "ret") {
        // 赋值右侧是 return value
        outmips << "move " << reg << ", $v1" << endl; // 约定函数返回值都放在 $v1寄存器中
    }
    else if (arg1.find("[") > 0) {
        string arr_name = arg1.substr(0, arg1.find("["));
        string index = arg1.substr(1+arg1.find("["));
        index = index.substr(0, index.size()-1); //
        cout << arr_name << "右侧 数组下标 index = " << index << " 赋值给左侧变量"<< endl;
        if (vars_addr.count(index) == 1) { // 右侧数组下标是局部(中间)变量 xx = num[#t2]
            int mid_var_index = vars_addr[index]; // 得到存变量的内存地址
            arg_offset = getReg(); // *4 的结果
            string mul_4 = getReg(); // 准备乘4
            if (lockedRegMap.count(index) == 1) { // 这个下标是 #tx，已经存在一个 $s 寄存器里，需要释放
                int sregNo = lockedRegMap[index];
                mul_4 = "$s" + to_string(sregNo); // 直接用这个寄存器
                lockedRegMap.erase(index);
                saveRegisterGroup[sregNo] = 0;  // 释放
            }
            else if (concludeVarMap.count(index) == 1 && func_attr.findArgumentPos(index) == 0) {
                mul_4 = concludeVarMap[index];  // 先考虑参数寄存器
            }
            else {
                outmips << "lw " << mul_4 << ", " << mid_var_index << "($sp)" << endl;
            }
            
            //然后再进行 下标*4, 左移2位
            outmips << "sll " << arg_offset << ", " << mul_4 << ", 2" << endl;
            if (vars_addr.count(arr_name)){ // 右侧是局部数组,  下标是(中间)局部变量
                addr_arg = vars_addr[arr_name]; // 局部数组相对于本函数的首地址
                string tmp_arg1_addr_reg = getReg(); // 用来存数组头地址
                outmips << "li " << tmp_arg1_addr_reg << ", " << addr_arg << endl;
                outmips << "add " << tmp_arg1_addr_reg << ", " << tmp_arg1_addr_reg << ", " << arg_offset << endl;
                // 上面得到 右侧数组元素 实际偏移地址大小，一个负整数，数组是往上长的，加offset正数
                //outmips << "TIPS: 右侧数组元素 实际偏移地址大小，一个负整数，数组是往上长的，加offset正数" << endl;
                outmips << "add " << tmp_arg1_addr_reg << ", " << tmp_arg1_addr_reg << ", $sp" << endl;
                outmips << "lw " << reg << ", (" << tmp_arg1_addr_reg << ")" << endl;
                // 把右侧要赋值的数装载到 reg 里就完事儿了
            }
            else if (global_vars.count(arr_name) == 1) { // 右侧是全局数组, 下标是(中间)局部变量
                //  lw $target, array($t1_offset)
                outmips << "lw " << reg << ", " << arr_name << "(" << arg_offset << ")" << endl;
            }
        }
        else if (int_cons_value.count(index) == 1) { // 下标是 int 常量名
            if (vars_addr.count(arr_name) == 1) {
                addr_arg = vars_addr[arr_name] + 4*int_cons_value[index];
                outmips << "lw " << reg << ", " << addr_arg << "($sp)" << endl;
            }
            else if (global_vars.count(arr_name) == 1) { //
                cout << "右侧是全局数组！下标是 局部 int 常量名" << endl;
                // lw tmp, arr_name, lw reg, off(tmp) 数组名直接表示地址
                int off = 4*int_cons_value[index];
                outmips << "lw " << reg << ", " << arr_name << "+" << off << endl;
            } else {
                cout << "右侧找不到该数组名 " << arr_name << endl;
            }
            
        }
        else if (global_int_cons_value.count(index) == 1) { //下标是全局int 常量名
            if (vars_addr.count(arr_name) == 1) {
                addr_arg = vars_addr[arr_name] + 4*global_int_cons_value[index];
                outmips << "lw " << reg << ", " << addr_arg << "($sp)" << endl;
            } else if (global_vars.count(arr_name) == 1) {
                cout << "右侧是全局数组！下标是 全局 int 常量名" << endl;
                int off = 4*global_int_cons_value[index];
                outmips << "lw " << reg << ", " << arr_name << "+" << off << endl;
            } else {
                cout << "右侧找不到该数组名 " << arr_name << endl;
            }
        }
        else if (global_vars.count(index) == 1) { // index 下标是全局变量
            arg_offset = getReg();
            outmips << "lw " << arg_offset << ", " << index << endl; // 直接用全局变量名字取地址取值
            outmips << "sll " << arg_offset << ", " << arg_offset << ", 2" << endl; // 下标*4
            if (vars_addr.count(arr_name)){
                addr_arg = vars_addr[arr_name]; // 局部数组相对于本函数的首地址，$sp时刻代表当前函数的起始位置
                string tmp_arg1_addr_reg = getReg(); // 用来存数组头地址
                outmips << "li " << tmp_arg1_addr_reg << ", " << addr_arg << endl;
                outmips << "add " << tmp_arg1_addr_reg << ", " << tmp_arg1_addr_reg << ", " << arg_offset << endl;
                // 上面得到 右侧数组元素 实际偏移地址大小，一个负整数，数组是往上长的，加offset正数
                outmips << "add " << tmp_arg1_addr_reg << ", " << tmp_arg1_addr_reg << ", $sp" << endl;
                outmips << "lw " << reg << ", (" << tmp_arg1_addr_reg << ")" << endl;
                
            } else if (global_vars.count(arr_name) == 1) { // 右侧是全局数组, 下标全局变量
                outmips << "lw " << reg << ", " << arr_name << "(" << arg_offset << ")" << endl;
            }
        }
        else { // arg1的下标是整数 要优先考虑是不是局部数据
            if (vars_addr.count(arr_name) == 1) {
                addr_arg = vars_addr[arr_name] + 4*stoi(index); // 问题，index有可能是 #tx
                outmips << "lw " << reg << ", " << addr_arg << "($sp)" << endl;
            } else if (global_vars.count(arr_name) == 1) { // 全局数组
                outmips << "lw " << reg << ", " << arr_name << "+" << to_string(4*stoi(index)) << endl;
            } else {
                cout << "找不到该数组名 " << arr_name << endl;
            }
        }
        // 装载reg完毕
    }
    else if (arg1 == "") {
        // do nothong
    } else
    { // 如果 arg1 == "" 不做任何处理
        cout << "type = 2 midCode 无法二元赋值" << endl;
    }
    return reg;
}


bool isInteger(string str){
    if (str[0] == '+' || str[0] == '-' || isdigit(str[0])) {
        return true;
    } else return false;
}

void storeA1() {
    string store_a = getReg(); // store_a是一个虚拟的位置标记寄存器，并未真正移动$sp
    outmips << "addi " << store_a << ", $sp, -52" << endl;  // 3*4 用来存原来是12位
    // 要存 $s0-$s9
    for (int i = 0; i <= 7; i++) { // $s0-$s7
        if (saveRegisterGroup[i] == 1) { // 被占用了，那么要保存现场
            outmips << "sw $s" << i << ", " << i*4 << "(" << store_a << ")" << endl;
        }
    }
    // 0,4, ..., 28
    // 32, 36, 40, 44, 48, 52
    for (int i = 0; i < concludeVarMap.size(); i++) { // 0~4 ,
        outmips << "sw $t" << i << ", " << 32+4*i << "(" << store_a << ")" << endl;
    }
}
void restoreA1() {

    for (int i = 0; i <= 7; i++) { // $s0-$s7
        if (saveRegisterGroup[i] == 1) { // 被占用了，那么要保存现场
            outmips << "lw $s" << i << ", " << i*4 << "($sp)" << endl;
        }
    }
    for (int i = 0; i < concludeVarMap.size(); i++) { // 0~4 ,
        outmips << "lw $t" << i << ", " << 32+4*i << "($sp)" << endl;
    }
    
    outmips << "addi $sp, $sp, 52" << endl;
}

int isPowerOf2(string num) {
    // 传进来的是 数字字符串
    if (isInteger(num)) {
        int tmp = stoi(num);
        if (tmp == 2) {
            return 1;
        } else if (tmp == 4) {
            return 2;
        } else if (tmp == 8) {
            return 3;
        } else if (tmp == 16) {
            return 4;
        } else if (tmp == 32) {
            return 5;
        } else if (tmp == 64) {
            return 6;
        } else if (tmp == 128) {
            return 7;
        } else return 0;
    }
    return 0; // 如果返回的是 0，说明不是2的幂
}

//
//  Generate_mid_code.cpp
//  代码生成
//
//  Created by 王珊珊 on 2019/11/14.
//  Copyright © 2019 vanellope. All rights reserved.
//
#include <string>
#include <iostream>
#include <vector>
#include "Mid_code_struct.h"

using namespace std;
vector<midCode> midCodeContainer;
string cur_temp_reg = "";
string last_temp_reg = "";
int temp_t = 0;     // t1, t2, t3...无脑递增
int label_t = 0;


void outputMid(midCode item){ // 生成四元式存到一个容器或者文件里
    midCodeContainer.push_back(item);   // 往里面装一个item
}

// for_1_begin, for_1_end,
// while_2_begin, while_2_end
// do_3_begin, do_3_end
// if_3_else, if_3_end
// 四元式构造器, type = 1, 4, 5, 6, type = 9 失效并直接略过
midCode::midCode(int type, string result, string arg1, string op, string arg2) {
    this->type = type;
    this->result = result;
    this->arg1 = arg1;
    this->op = op;
    this->arg2 = arg2;
}
//二元 type = 2, 7
midCode::midCode(int type, string result, string arg1){
    this->type = type;
    this->result = result;
    this->arg1 = arg1;
//    if (type == 2 && (result == "int" || result == "char" || result == "void")) {
//        temp_t = 0;
//    } // 进入一个新的函数，中间变量可以从头开始#t1，#t2，#t3
}

// 三元 type = 3
midCode::midCode(int type, string result, string arg1, string op){
    this->type = type;
    this->result = result;
    this->arg1 = arg1;
    this->op = op;
}

// 一元 type = 8, label
midCode::midCode(int type, string result){
    this->type = type;
    this->result = result;
}

// for_1_begin, for_1_end,
// while_2_begin, while_2_end
// do_3_begin, do_3_end
// if_3_else, if_3_end
pair<string, string> genLabel(string type){
    string newLabelHead = "";
    string newLabelTail = "";
    label_t++;
    if (type == FOR) {
        newLabelHead = newLabelHead + "for_" + to_string(label_t) + "_begin:";
        newLabelTail = newLabelTail + "for_" + to_string(label_t) + "_end:";
    } else if (type == DO) {
        newLabelHead = newLabelHead + "do_" + to_string(label_t) + "_begin:";
        newLabelTail = newLabelTail + "do_" + to_string(label_t) + "_end:";
    } else if (type == WHILE) {
        newLabelHead = newLabelHead + "while_" + to_string(label_t) + "_begin:";
        newLabelTail = newLabelTail + "while_" + to_string(label_t) + "_end:";
    } else if (type == IF) {
        newLabelHead = newLabelHead + "if_" + to_string(label_t) + "_else:"; // 不满足条件则跳转到这里
        newLabelTail = newLabelTail + "if_" + to_string(label_t) + "_end:";  // 满足条件执行完跳到这里
    } else if (type == FUNC) { // 这个根本没用到
        newLabelHead = newLabelHead + "func_" + to_string(label_t) + "_begin:";
        newLabelTail = newLabelTail + "func_" + to_string(label_t) + "_end:";
    } else {
        cout << "Cannot generate a pair of label" << endl;
    }
    return make_pair(newLabelHead, newLabelTail);
}

string genTempReg(){    // t1, t2, t3, t4 ...返回一个临时中间变量名,无限递增
    last_temp_reg = cur_temp_reg;
    string tempReg = "#t";
    temp_t++;
    tempReg = tempReg + to_string(temp_t);
    cur_temp_reg = tempReg; // 更新全局变量 最新的临时中间变量寄存器
    return tempReg;
}
// 打印中间语句 连接功能测试成功
void printMid() {
    for (int i = 0; i < midCodeContainer.size(); i++) {
        if (midCodeContainer[i].type != 9) {
            cout << "type = " << midCodeContainer[i].type << ", ";
            cout << midCodeContainer[i].result << " ";
            cout << midCodeContainer[i].arg1 << " ";
            cout << midCodeContainer[i].op << " ";
            cout << midCodeContainer[i].arg2 << endl;
        }
       
    }
}

string getCurTempReg(){
    return cur_temp_reg;
}

string getLastTempReg(){
    return last_temp_reg;
}
int midCodeSize() {
    return midCodeContainer.size();
}
midCode getNextMidCode(int i){
    return midCodeContainer[i];
}

void modifyMidCode(midCode code, int pos) {
    midCodeContainer[pos] = code;
    cout << "修改midCode ，result : " << code.result << ", arg1: " << code.arg1
     << ", op: " << code.op << ", arg2: "<< code.arg2 << endl;
}

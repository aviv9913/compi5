//
// Created by Adina Katz on 23/06/2021.
//

#ifndef COMPI5_IRMANAGER_HPP
#define COMPI5_IRMANAGER_HPP

#include "hw3_output.hpp"
#include "bp.hpp"
#include "library.h"

using namespace std;
typedef pair<int, BranchLabelIndex> bp_pair;

int emit(const string &s) {
    return CodeBuffer::instance().emit(s);
}

void emitGlobal(const string &s) {
    CodeBuffer::instance().emitGlobal(s);
}

void bpatch(const vector<bp_pair> &list, const string &label) {
    CodeBuffer::instance().bpatch(list, label);
}

string genLabel() {
    return CodeBuffer::instance().genLabel();
}

vector<bp_pair> makeList(bp_pair item) {
    return CodeBuffer::instance().makelist(item);
}

vector<bp_pair> merge(const vector<bp_pair> &list1, const vector<bp_pair> &list2) {
    return CodeBuffer::instance().merge(list1, list2);
}

void printCodeBuffer() {
    return CodeBuffer::instance().printCodeBuffer();
}

void printGlobalBuffer() {
    return CodeBuffer::instance().printGlobalBuffer();
}

string getReg() {
    static int reg_count = 0;
    return "%t" + to_string(reg_count++);
}

string getNewStr() {
    static int str_count = 0;
    return "@.Str" + to_string(str_count++);
}

string getRELOPType(string op, string isize) {
    if (op == "==") return "icmp eq i32";
    if (op == "!=") return "icmp ne i32";
    if (op == "<") return (isize == "i8") ? "icmp ult i8" : "icmp slt i32";
    if (op == ">") return (isize == "i8") ? "icmp ugt i8" : "icmp sgt i32";
    if (op == "<=") return (isize == "i8") ? "icmp ule i8" : "icmp sle i32";
    if (op == ">=") return (isize == "i8") ? "icmp uge i8" : "icmp sge i32";
    return "";
}

string getArithmeticType(string op, string isize) {
    if (op == "+") return "add i32";
    if (op == "-") return "sub i32";
    if (op == "*") return "mul i32";
    if (op == "/") return (isize == "i8") ? "udiv i8" : "sdiv i8";
    return "";
}

string getIRSize(string left_type, string right_type) {
    string isize;
    if (left_type == "INT" || right_type == "INT") {
        isize = "i32";
    } else {
        isize = "i8";
    }
    return isize;
}

void exitProgram() {
    emit("call void @exit(i32 1)");
}

int emitConditionFromResult(string res) {
    return emit("br i1 " + res + ", label @, label @");
}

int emitUnconditional() {
    emit("br label @");
}

int emitCondition(string reg1, string relop, string reg2) {
    string res_reg = getReg();
    emit(res_reg + " = " + getRELOPType(op) + reg1 + ", " + reg2);
    return emitConditionFromResult(res_reg);
}

class IRManager {
private:
    void addExitAndPrintFunctions() {
        emit("declare i32 @printf(i8*, ...)");
        emit("declare void @exit(i32)");
        emitGlobal("@.int_specifier = constant [4 x i8] c\"%d\\0A\\00\"");
        emitGlobal("@.str_specifier = constant [4 x i8] c\"%s\\0A\\00\"");

        emit("define void @printi(i32) {");
        emit("call i32 (i8*, ...) @printf(i8* getelementptr ([4 x i8], [4x i8]* @.int_specifier, i32 0, i32 0), i32 %0)");
        emit("ret void");
        emit("}");
        emit("define void @print(i8*) {");
        emit("call i32 (i8*, ...) @printf(i8* getelementptr ([4 x i8], [4x i8]* @.str_specifier, i32 0, i32 0), i8* %0)");
        emit("ret void");
        emit("}");
    }

    void handleZeroDivision(string reg) {
        int loc = emitCondition("0", "!=", reg);
        string bad_zero_label = genLabel();
        string error_msg = addGlobalString("Error division by zero");
        emit("call void @print(i8* " + error_msg + ")");
        exitProgram();
        int end_error = emitUnconditional();
        string good_zero_label = genLabel();
        bpatch(makeList(bp_pair(loc, FIRST)), good_zero_label);
        bpatch(makeList(bp_pair(end_error, FIRST)), good_zero_label);
        bpatch(makeList(bp_pair(loc, SECOND)), bad_zero_label);
    }

public:
    IRManager() { addExitAndPrintFunctions(); }

    string addGlobalString(string s){
        string new_str= freshString();
        string string_size= to_string(s.length() + 1);
        emitGlobal(new_str + " = constant [" + string_size + "x i8] c\"" + s + "\\00\"");
        string reg = getReg();
        emit(reg + " = getelementptr [" + string_size + "x i8] , ["+ string_size + "x i8] * " +
                new_str + ", i32 0, i32 0");
        return reg;
    }



};


#endif //COMPI5_IRMANAGER_HPP

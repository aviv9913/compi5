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

vector<bp_pair> ll(bp_pair item) {
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

string getGlobalReg(string reg) {
    string global_reg = reg.replace(0, 1, "@");
    return global_reg;
}

string getNewStr() {
    static int str_count = 0;
    return "@.Str" + to_string(str_count++);
}

string getRELOPType(string op, bool isSigned) {
    if (op == "==") return "icmp eq i32";
    if (op == "!=") return "icmp ne i32";
    if (op == "<") return isSigned ? "icmp ult i8" : "icmp slt i32";
    if (op == ">") return isSigned ? "icmp ugt i8" : "icmp sgt i32";
    if (op == "<=") return isSigned ? "icmp ule i8" : "icmp sle i32";
    if (op == ">=") return isSigned ? "icmp uge i8" : "icmp sge i32";
    return "";
}

string getArithmeticType(string op, bool isSigned) {
    if (op == "+") return "add i32";
    if (op == "-") return "sub i32";
    if (op == "*") return "mul i32";
    if (op == "/") return isSigned ? "udiv i8" : "sdiv i32";
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
    emit("call void @exit(i32 0)");
}

int emitConditionFromResult(string res) {
    return emit("br i1 " + res + ", label @, label @");
}

int emitUnconditional() {
    return emit("br label @");
}

int emitCondition(string reg1, string relop, string reg2) {
    string res_reg = getReg();
    emit(res_reg + " = " + getRELOPType(relop) + reg1 + ", " + reg2);
    return emitConditionFromResult(res_reg);
}

int emitZext(string reg1, string reg2) {
    return emit(reg1 + " = zext i8 " + reg2 + " to i32");
}

int emitZext(Exp *left, Exp *right) {
    if (left->type == "BYTE") {
        string reg_l = getReg();
        return emitZext(reg_l, left->reg);
    } else if (right->type == "BYTE") {
        string reg_r = getReg();
        return emitZext(reg_r, right->reg);
    }
    return -1; // shouldn't get here
}

int emitPhi(string reg1, string reg2, string value, string label) {
    return emit(reg1 + " = phi i1 [" + reg2 + ", " + value + "],[0, " + label + "]");
}

void emitStore(string dataReg, string ptrReg, string ptrRegType= "i32*", string dataRegType="i32"){
    if(ptrRegType.find("*") == string::npos)
    {
        cerr<<"emitStore: no '*' in ptrReg type";
        exit(1);
    }
    emit("store " + dataRegType +" %" + dataReg + ", " + ptrRegType +" %" + ptrReg);
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
        emitCondition(reg, "=", "0");
        int b_first = emitConditionFromResult(reg);
        string l_first = genLabel();
        string zero_reg = addGlobalString("Error division by zero");
        emit("call void @print(i8* " + zero_reg + ")");
        exitProgram();
        int b_second = emitUnconditional();
        string l_second = genLabel();
        bpatch(makeList(bp_pair(b_first, FIRST)), l_first);
        bpatch(makeList(bp_pair(b_first, SECOND)), l_second);
        bpatch(makeList(bp_pair(b_second, FIRST)), l_second);
    }

public:
    IRManager() { addExitAndPrintFunctions(); }
    static IRManager &instance(){
        static IRManager inst;
        return inst;
    }

    string addGlobalString(string s){
//        string new_str= freshString();
        string string_size= to_string(s.length() + 1);
        string reg = getReg();
        string global_reg = getGlobalReg(reg);
        emitGlobal(global_reg + " = constant [" + string_size + "x i8] c\"" + s + "\\00\"");
        emit(reg + " = getelementptr [" + string_size + "x i8] , ["+ string_size + "x i8]* " +
                reg + ", i32 0, i32 0");
        return reg;
    }

    string assignToReg(string value, bool isSigned, string reg = "") {
        if (reg.empty()) {
            reg = getReg();
        }
        if (isSigned) {
            emit(reg + " = add i8 0," + value);
        } else {
            emit(reg + " = add i32 0," + value);
        }
        return reg;
    }

    string assignToReg(string value, string type="i32", string reg = "") {
        if (reg.empty()) {
            reg = getReg();
        }
        emit(reg + " = add " + type + " 0," + value);
        return reg;
    }

    //TODO: how we are using this? seems like value should contain '%'
    string assignBoolToReg(string value, string reg = "") {
        if (reg.empty()) {
            reg = getReg();
        }
        if (value.find("%") == 0) { // find returns the pos of the symbol
            emit(reg + " = add i1 1, " + value);
        } else {
            emit(reg + " = add i1 0," + value);
        }
        return reg;
    }

    string relop(Exp *left, Exp *right, string op_type, bool isSigned) {
        string op = getRELOPType(op_type, isSigned);
        string reg_l = left->reg;
        string  reg_r = right->reg;
        string new_reg = getReg();

        if (!isSigned) {
            if (left->type == "BYTE") {
                reg_l = getReg();
                emitZext(reg_l, left->reg);
            } else if (right->type == "BYTE") {
                reg_r = getReg();
                emitZext(reg_r, right->reg);
            }
        }
        emit(new_reg + " " + op + " " + reg_l + ", " + reg_r);
        return new_reg;
    }

    string binop(Exp *left, Exp *right, string op_type, bool isSigned) {
        string op = getArithmeticType(op_type, isSigned);
        string reg_l = left->reg;
        string reg_r = right->reg;
        string new_reg = getReg();

        if (op_type == "/") {
            emitZext(left, right);
            handleZeroDivision(right->reg);
        }
        if (!isSigned) {
            emitZext(left, right);
        }
        emit(new_reg + " = " + op + " " + reg_l + ", " + reg_r);
        if (!isSigned && right->type == "BYTE" && left->type == "BYTE") {
            reg_r = getReg();
            emit(reg_r + " = trunc i32 " + new_reg + " to i8");
            new_reg = reg_r;
        }
        return new_reg;
    }

    vector<string> boolop(string left_reg, string right_reg, string op_type, string instr, P *short_circuit){
        vector<string> res;
        string new_reg = getReg();
        string first_l;
        string second_l;
        string end_l = genLabel();
        int loc2 = emitUnconditional();
        int loc3 = emitUnconditional();

        if (op_type == "and") {
            second_l = genLabel(); // left false
            emitPhi(new_reg, right_reg, instr, second_l);
            first_l = short_circuit->instruction;
        } else if (op_type == "or") {
            first_l = genLabel(); // left true
            emitPhi(new_reg, right_reg, instr, first_l);
            second_l = short_circuit->instruction;
        }
        bpatch(makeList(bp_pair(short_circuit->loc, FIRST)), first_l);
        bpatch(makeList(bp_pair(short_circuit->loc, SECOND)), second_l);
        bpatch(makeList(bp_pair(loc2, FIRST)), end_l);
        bpatch(makeList(bp_pair(loc3, FIRST)), end_l);
        
        res.push_back(new_reg);
        res.push_back(end_l);
        return res;
    }

    void emitGetElementPtr(string elementReg, string ptrVar, int size, int index){
        emit(
            "%" + elementReg + " = getelementptr [" + to_string(size) + " x i32]," +
            " [" + to_string(size) + " x i32]* %" + ptrVar + "," +
            "i32 0, i32 " + to_string(index)
        );
    }
};


#endif //COMPI5_IRMANAGER_HPP

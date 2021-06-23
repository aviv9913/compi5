//
// Created by Aviv on 17/05/2021.
//

#include "library.h"
#include <cstring>
#include <sstream>
extern char *yytext;

/// Global variables
string currentFunc;
int currentFuncArgs;
int loopCount = 0;
int insideCase = 0;

/// Table Stack
vector<shared_ptr<SymbolTable>> tableStack;
vector<int> offsetStack;

/// Code Buffer
CodeBuffer &buffer = CodeBuffer::instance();

/// Registers
//TODO

void printTableStack(){
    for(int i=0; i<tableStack.size(); i++){
        auto table = tableStack[i];
        for(int j=0; j<(table->symbols).size(); j++){
            auto symbol = (table->symbols)[j];
            cout<<"symbol:"<< symbol->name <<' '<<symbol->type[0]<<" "<<symbol->offset<<endl;
        }
    }
}

void incCase() {
    FUNC_ENTRY()
    insideCase++;
}

void decCase() {
    FUNC_ENTRY()
    insideCase--;
}

void incLoop() {
    loopCount++;
}
//TODO: add M, N, P to parser
void decLoop(N *first, P *second, Statement *st) {
    loopCount--;

    int new_loc = buffer.emit("br label @");
    string new_label = buffer.genLabel();
    buffer.bpatch(buffer.makelist({first->loc, FIRST}), first->code);
    buffer.bpatch(buffer.makelist({second->loc, FIRST}), second->code);
    buffer.bpatch(buffer.makelist({second->loc, SECOND}), new_label);
    buffer.bpatch(buffer.makelist({new_loc, FIRST}), first->code);

    if (st->breakList.size() != 0) {
        buffer.bpatch(st->breakList, new_label);
    }
    if (st->continueList.size() != 0) {
        buffer.bpatch(st->continueList, first->code);
    }
}

void openScope(){
    DEBUG(cout<<"openScope"<<endl;)
    auto newScope = shared_ptr<SymbolTable>(new SymbolTable);
    tableStack.emplace_back(newScope);
    offsetStack.push_back(offsetStack.back());
}

void closeScope(){
    output::endScope();
    auto scope_table = tableStack.back();
    for (auto i:scope_table->symbols) {//printing all the variables and functions
        if (i->type.size() == 1) { //variable
//            output::printID(i->name, i->offset, i->type[0]);
        } else {
            auto retVal = i->type.back();
            i->type.pop_back();
            if (i->type.front() == "VOID") {
                i->type.pop_back();
            }
            //functions
//            output::printID(i->name, i->offset, output::makeFunctionType(retVal, i->type));
        }
    }

    //clear symbol table
    while (scope_table->symbols.size() != 0) {
        scope_table->symbols.pop_back();
    }
    tableStack.pop_back();
    offsetStack.pop_back();
    DEBUG(printTableStack();)
    FUNC_EXIT()
}

void endProgram() {
    FUNC_ENTRY()
    auto global = tableStack.front()->symbols;
    bool mainFound = false;
    for (int i = 0; i < global.size(); ++i) {
        DEBUG(cout<<global[i]->name<<", "<<global[i]->type.size()<<" ,"<< global[i]->type[0]<<endl;)
        if (global[i]->name == "main") {
            if (global[i]->type.size() == 2) {
                if (global[i]->type[0] == "VOID" && global[i]->type[1] == "VOID") {
                    mainFound = true;
                    break;
                }
            }
        }
    }
    if (!mainFound) {//no main function
        output::errorMainMissing();
        exit(0);
    }
    closeScope();
    buffer.printCodeBuffer();
    buffer.printGlobalBuffer();
}

void endCurrentFunc(RetType *retType) {
    if (retType->value == "VOID") {
        buffer.emit("ret void");
    } else {
        string type = getLLVMType(retType->value);
        buffer.emit("ret " + type + "0");
    }
    buffer.emit("}");
    currentFunc = "";
    currentFuncArgs = 0;
}

void enterArgsToStackTable(Formals *fm) {
    DEBUG(cout<<"enterArgsToStackTable:"<<endl;)
    for (int i = 0; i < fm->formals.size(); ++i) {
        auto temp = shared_ptr<SymbolEntry>(new SymbolEntry(fm->formals[i]->value, fm->formals[i]->type,-i-1));
        tableStack.back()->symbols.push_back(temp);
    }
    DEBUG(printTableStack();)
}

bool IDExists(string id) {
    DEBUG(printTableStack();)
    DEBUG(cout<<"IDExists: checking-"<<id<<endl;)
    for (int i = tableStack.size()-1 ; i>=0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name.compare(id) == 0) {
                return true;
            }
        }
    }
    DEBUG(cout<<"IDExists:"<<id<<" Not found"<<endl;)
    return false;
}

string getLLVMType(string type) {
    if (type == "VOID") {
        return "void";
    }
    if (type == "BOOL") {
        return "i1";
    }
    if (type == "BYTE") {
        return "i8";
    }
    if (type == "STRING") {
        return "i8*";
    }
    return "i32"; // int
}
//TODO: add all below to library.h
string prepareCommandForEmit(bool isGlobal, string reg, string type, string value = "", int size = 0) {
    std::stringstream command;
    if (isGlobal){
        if (type.compare("STRING") == 0) {
            command << "@";
            command << reg;
            command << "= constant [";
            command << to_string(size);
            command << " x i8] c\"";
            command << value;
            command << "\"";
            string ret(command.str());
            return ret;
        }
    } else {
        if (type.compare("STRING") == 0) {
            command << "%";
            command << reg;
            command << "= getelementptr [";
            command << to_string(size);
            command << " x i8], [";
            command << to_string(size);
            command << " x i8]* @";
            command << reg;
            command << ", i8 0, i8 0";
            string ret(command.str());
            return ret;
        } else if (type.compare("INT") == 0) {
            command << "%";
            command << reg;
            command << " = add i32 0,";
            command << value;
            string ret(command.str());
            return ret;
        } else if (type.compare("BYTE") == 0) {
            command << "%";
            command << reg;
            command << " = add i8 0,";
            command << value;
            string ret(command.str());
            return ret;
        } else if (type.compare("BOOL") == 0) {
            command << "%";
            command << reg;
            command << " = add i1 ";
            if (value.compare("true") == 0) {
                command << "0,1";
            } else if (value.compare("false") == 0) {
                command << "0,0";
            } else {
                command << "1, %";
                command << value;
            }
            string ret(command.str());
            return ret;
        }
    }
    return ""; // shouldn't get here
}

string prepareRELOPCommandForEmit(string reg1, string reg2, string isize, string reg3 = "", string relop = ""){
    std::stringstream command;
    if (relop.empty() && reg3.empty()) {
        command << "%";
        command << reg1;
        command << " = zext i8 %";
        command << reg2;
        command << " to ";
        command << isize;
        string ret(command.str());
        return ret;
    } else {
        command << "%";
        command << reg1;
        command << " = icmp ";
        command << relop;
        command << " ";
        command << isize;
        command << " %";
        command << reg2;
        if (reg3.compare("cond") == 0) {
            command << ", 0";
        } else {
            command << ", %";
            command << reg3;
        }
        string ret(command.str());
        return ret;
    }
}

string prepareBranchCommand(string reg = "") {
    std::stringstream command;
    if (reg.empty()) {
        command << "br label @";
    } else {
        command << "br i1 %";
        command << reg;
        command << ", label @, label @";
    }
    string ret(command.str());
    return ret;
}

/************************ CALL ************************/
// Call: ID()
Call::Call(Node *ID) {
    FUNC_ENTRY()
    auto global_scope = tableStack.front()->symbols;
    for (auto i:global_scope) {
        if (i->name == ID->value) {
            if (i->type.size() == 1) {
                output::errorUndefFunc(yylineno, ID->value);
                exit(0);
            }
            if (i->type.size() == 2) {
                this->value = i->type.back();
                return; //if we reached this line, then we found the right function
            } else {
                vector<string> empty = {""};
                output::errorPrototypeMismatch(yylineno, i->name, empty);
                exit(0);
            }
        }
    }
    output::errorUndefFunc(yylineno, ID->value); //shouldn't reach this line if the function exist
    exit(0);
}

// Call: ID(ExpList)
Call::Call(Node *ID, ExpList *paramList) {
    FUNC_ENTRY()
    DEBUG(for(auto exp:paramList->expList){
        cout<<exp.value<<","<<exp.type<<endl;
        }
    )
    auto global_scope = tableStack.front()->symbols;
    for (auto i:global_scope) {
        if (i->name == ID->value) {
            if (i->type.size() == 1) {
                output::errorUndefFunc(yylineno, ID->value);
                exit(0);
            }
            if (i->type.size() == 1 + paramList->expList.size()) {
                for (int j = 0; j < paramList->expList.size(); ++j) {
                    if (paramList->expList[j].type == "BYTE" && i->type[j] == "INT") {
                        continue;
                    }
                    if (paramList->expList[j].type != i->type[j]) {
                        i->type.pop_back(); // removing the return type of the function
                        output::errorPrototypeMismatch(yylineno, i->name, i->type);
                        exit(0);
                    }
                }
                this->value = i->type.back();
                return; //if we reached this line, then we found the right function
            } else {
                i->type.pop_back();
                output::errorPrototypeMismatch(yylineno, i->name, i->type);
                exit(0);
            }
        }
    }
    output::errorUndefFunc(yylineno, ID->value); //shouldn't reach this line if the function exist
    exit(0);
}

/************************ EXP ************************/
// NUM, NUM B, STRING, TRUE, FALSE
Exp::Exp(Node *terminal, string str) : Node(terminal->value) {
    FUNC_ENTRY()
    DEBUG(cout<<str<<endl;)
    this->type = "";
    this->boolValue = false;
    vector<pair<int, BranchLabelIndex>> false_list;
    vector<pair<int, BranchLabelIndex>> true_list;
    this->falseList = false_list;
    this->trueList = true_list;

    if (str.compare("NUM") == 0) {
        this->type = "INT";
        this->reg = pool.getReg();
        string command = prepareCommandForEmit(false, this->reg, this->type, terminal->value);
        buffer.emit(command);
    } else if (str.compare("B") == 0) {
        if (stoi(terminal->value) > 255) {
            output::errorByteTooLarge(yylineno, terminal->value);
            exit(0);
        }
        this->type = "BYTE";
        this->reg = pool.getReg();
        string command = prepareCommandForEmit(false, this->reg, this->type, terminal->value);
        buffer.emit(command);
    } else if (str.compare("STRING") == 0) {
        this->type = "STRING";
        this->reg = pool.getReg();
        terminal->value[terminal->value.size() - 1] = '\00';
        string command = prepareCommandForEmit(false, this->reg, this->type, "", terminal->value.size());
        string command_global = prepareCommandForEmit(true, this->reg, this->type, terminal->value,
                                                      terminal->value.size());
        buffer.emit(command);
        buffer.emit(command_global);
    } else if (str.compare("TRUE") == 0) {
        this->boolValue = true;
        this->type = "BOOL";
        this->reg = pool.getReg();
        string command = prepareCommandForEmit(false, this->reg, this->type, "true");
        buffer.emit(command);
    } else {
        this->boolValue = false;
        this->type = "BOOL";
        string command = prepareCommandForEmit(false, this->reg, this->type, "false");
        buffer.emit(command);
    }

}

// !Exp
Exp::Exp(Node *Not, Exp *exp) {
    FUNC_ENTRY()
    this->type = "";
    if (exp->type.compare("BOOL") != 0) {
        output::errorMismatch(yylineno);
        exit(0);
    }
    this->type = "BOOL";
    this->boolValue = !exp->boolValue;
    this->reg = pool.getReg();
    this->falseList = exp->trueList;
    this->trueList = exp->falseList;
    string command = prepareCommandForEmit(false, this->reg, this->type, to_string(exp->reg));
    buffer.emit(command);
}
//TODO: update parser
// Exp op Exp
Exp::Exp(Exp *left, Node *op, Exp *right, string str, P *shortC) {
    FUNC_ENTRY()
    this->type = "";
    this->boolValue = false;
    this->reg = pool.getReg();
    vector<pair<int, BranchLabelIndex>> false_list;
    vector<pair<int, BranchLabelIndex>> true_list;
    this->falseList = false_list;
    this->trueList = true_list;
    string end_instr;

    DEBUG(cout<<"left:"<<left->type<<", op:"<<op->value<<", right:"<<right->type<<", srting:"<<str<<endl;)
    // RELOP checking if both exp are numbers
    if ((left->type.compare("INT") == 0 || left->type.compare("BYTE") == 0) &&
        (right->type.compare("INT") == 0 || right->type.compare("BYTE") == 0)) {
        string isize = findIRSize(left->type, right->type);
        string dataRegL = left->reg;
        string dataRegR = right->reg;
        if (str.compare("RELOPL") == 0 || str.compare("RELOPN") == 0) {
            this->type = "BOOL";
            string relop = findRELOPType(op->value, isize);
            if (isize.compare("i32") == 0) {
                if (left->type.compare("BYTE") == 0) {
                    dataRegL = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegL, left->reg, isize);
                    buffer.emit(command);
                }
                if (right->type.compare("BYTE") == 0) {
                    dataRegR = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegR, right->reg, isize);
                    buffer.emit(command);
                }
            }
            string command = prepareRELOPCommandForEmit(this->reg, dataRegL, isize, dataRegR, relop);
            buffer.emit(command);
            if (!right->instruction.empty()) {
                end_instr = right->instruction;
            } else {
                end_instr = left->instruction;
            }

        } // BINOP
        else if (str.compare("ADD") == 0 || str.compare("MUL") == 0) {
            this->type = "INT";
            if (left->type.compare("BYTE") == 0 && right->type.compare("BYTE")) {
                this->type = "BYTE"; //
            }
            this->reg = pool.getReg();
            string op_type = findOPType(op->value);
            if (op_type.compare("div") == 0) {
                string cond = pool.getReg();
                if (right->type.compare("BYTE") == 0) {
                    dataRegR = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegR, right->reg, isize);
                }
                if (left->type.compare("BYTE") == 0) {
                    dataRegL = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegL, left->reg, isize);
                }
                string command = prepareRELOPCommandForEmit(cond, dataRegR, isize, "cond", "eq");
                buffer.emit(command);
                command = prepareBranchCommand(cond);
                int loc_B_first = buffer.emit(command);
                string Lfirst = buffer.genLabel();
                string zero_reg = pool.getReg();
                // handling div by 0
                buffer.emit("%" + zero_reg + " = getelementptr [22 x i8], [22 x i8]* @DivByZeroExcp, i32 0, i32 0");
                buffer.emit("call void @print(i8* %" + zero_reg + ")");
                buffer.emit("call void @exit(i32 0)");
                command = prepareBranchCommand();
                int loc_B_second = buffer.emit(command);
                string Lsecond = buffer.genLabel();
                buffer.bpatch(buffer.makelist({loc_B_first, FIRST}), Lfirst);
                buffer.bpatch(buffer.makelist({loc_B_first, SECOND}), Lsecond);
                buffer.bpatch(buffer.makelist({loc_B_second, FIRST}), Lsecond);
                end_instr = Lsecond;
            }
            if (isize.compare("i32") == 0) {
                if (left->type.compare("BYTE") == 0) {
                    dataRegL = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegL, left->reg, isize);
                    buffer.emit(command);
                }
                if (right->type.compare("BYTE") == 0) {
                    dataRegR = pool.getReg();
                    string command = prepareRELOPCommandForEmit(dataRegR, right->reg, isize);
                    buffer.emit(command);
                }
            }
            string command = prepareRELOPCommandForEmit(this->reg, dataRegL, isize, dataRegR, op_type);
            buffer.emit(command);
            if (op_type.compare("div") == 0 && right->type.compare("BYTE") == 0 && left->type.compare("BYTE") == 0) {
                string data_reg = pool.getReg();
                buffer.emit("%" + data_reg + " = trunc i32 %" + this->reg + " to i8");
                this->reg = data_reg;
            }
        }
    } // AND, OR
    else if (left->type.compare("BOOL") == 0 &&
            right->type.compare("BOOL") == 0) {
        this->type = "BOOL";
        // need to evaluate the bool value of left and right
        if (str.compare("AND") == 0 &&
            op->value.compare("and") == 0) {
            this->boolValue = left->boolValue && right->boolValue;
        } else if (str.compare("OR") == 0 &&
                    op->value.compare("or") == 0) {
            this->boolValue = left->boolValue || right->boolValue;
        } else {
            output::errorMismatch(yylineno);
            exit(0);
        }
    }
    else {
        output::errorMismatch(yylineno);
        exit(0);
    }
    DEBUG(cout<<"type after calc:"<<this->type<<endl;)
}

// (Exp)
Exp::Exp(Exp *exp) {
    FUNC_ENTRY()
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
}

// ID
Exp::Exp(Node *ID) {
    FUNC_ENTRY()
    for (int i = tableStack.size()-1; i > 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == ID->value) {
                this->value = ID->value;
                this->type = tableStack[i]->symbols[j]->type.back();
                this->boolValue = false;
                return;
            }
        }
    } // if we didn't find the variable name in any of the scopes
    output::errorUndef(yylineno, ID->value);
    exit(0);
}

// Call
Exp::Exp(Call *call) {
    FUNC_ENTRY()
    this->type = call->value;
    this->boolValue =false;
}

// bool check
Exp::Exp(Exp *exp, string str) {
    DEBUG(cout<<"bool check"<<endl;)
    if (exp->type != "BOOL") {
        output::errorMismatch(yylineno);
        exit(0);
    }
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
}


// int check
Exp::Exp(Exp *exp, int num) {
    FUNC_ENTRY()
    DEBUG(cout<<"type:"<<exp->type<<endl;)
    if (exp->type.compare("INT") !=0 && exp->type.compare("BYTE") !=0) {
        output::errorMismatch(yylineno);
        exit(0);
    }
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
}

// Exp
ExpList::ExpList(Exp *exp) {
    FUNC_ENTRY()
    this->expList.emplace(expList.begin(), exp);
}

// Exp, ExpList
ExpList::ExpList(Exp *exp, ExpList *paramList) {
    FUNC_ENTRY()
    this->expList = paramList->expList;
    this->expList.emplace(expList.begin(), exp);
}

/************************ FUNC & FORMALS ************************/
// FuncDecl
FuncDecl::FuncDecl(RetType *retType, Node *ID, Formals *args) {
    FUNC_ENTRY()
    // checking if func already exists
    if (IDExists(ID->value)) {
        output::errorDef(yylineno, ID->value);
        exit(0);
    }

    // checking that param names doesn't equal func name
    for (int i = 0; i < args->formals.size(); ++i) {
        if (IDExists(args->formals[i]->value) ||
            args->formals[i]->value.compare(ID->value) == 0) {
            output::errorDef(yylineno, args->formals[i]->value);
            exit(0);
        }
        // checking that 2 params don't have the same name
        for (int j = i + 1; j < args->formals.size(); ++j) {
            if (args->formals[i]->value.compare(args->formals[j]->value) == 0) {
                output::errorDef(yylineno, args->formals[i]->value);
                exit(0);
            }
        }
    }
    this->value = ID->value;
    string argString = "(";
    //adding formals to symbol table and creating argString for the LLVM code
    if (args->formals.size() != 0) {
        for (int i = 0; i < args->formals.size(); ++i) {
            this->types.push_back(args->formals[i]->type);
            argString += getLLVMType(args->formals[i]->type) + ",";
        }
        argString.back() = ')'; // args is "(type name,type name)"
    }
    else{
        this->types.emplace_back("VOID");
        argString += ")";
    }
    DEBUG(cout<<"Arguments string: "<<argString<<end;)
    this->types.emplace_back(retType->value);
    // open new scope for func

    auto func_scope = shared_ptr<SymbolEntry>(new SymbolEntry(this->value, this->types, 0));
    tableStack[0]->symbols.push_back(func_scope);
    currentFunc = this->value;
    currentFuncArgs = args->formals.size();
    string retTypeString = getLLVMType(retType->value);
    //define i8 @function(i8,i8) {
    buffer.emit(
      "define " + retTypeString + " @"+ this->value + argString + " {"
    );
    //%stack = alloca [50 x i32] max 50 args on stack
    buffer.emit("%stack = alloca [50 x i32]");
    //%args = alloca [<size> x i32]
    buffer.emit("%args = alloca [" + to_string(args->formals.size()) + " x i32]");
    //TODO: reg pool
    for(int i=0; i<args->formals.size(); ++i){
        string ptrReg = pool.getReg();
        //%reg = getelementptr [<size> x i32],[<size> x i32]* %argsArrayPtr, i32 0, i32 <argNum>
        buffer.emit(
                "%" + ptrReg + " = getelementptr [" + to_string(size) + " x i32]," +
                " [" + to_string(size) + " x i32]* %args," +
                "i32 0, i32 " +    to_string(currFuncArgs - i - 1));
                );
        string dataReg = to_string(i);
        string argType = getLLVMType(args->formals[i]->type);
        if(argType != "i32"){
            dataReg = pool.getReg();
            //%reg = zext i8 %reg to i32
            buffer.emit(
                    "%" + dataReg + " = zext " + argType + " %" + to_string(i) + " to i32";
                    )
        }
        //store i32 %reg, i32* %ptr
        buffer.emit("store i32 %" + dataReg + ", i32* %" + ptrReg)
    }
}

// FormalsList-> FormalDecl
FormalsList::FormalsList(FormalDecl *formal) {
    FUNC_ENTRY()
    this->formals.insert(this->formals.begin(), formal);
}

// FormalsList-> FormalDecl, FormalsList
FormalsList::FormalsList(FormalDecl *formal, FormalsList *formalsList) {
    FUNC_ENTRY()
    this->formals = vector<FormalDecl *>(formalsList->formals);
    this->formals.insert(this->formals.begin(), formal);
}

// Formals-> FormalsList
Formals::Formals(FormalsList *formalsList) {
    FUNC_ENTRY()
    this->formals = vector<FormalDecl*>(formalsList->formals);
}

/************************ STATEMENT ************************/
// Statement-> (Statement)
Statement::Statement(Statements *sts) {
    FUNC_ENTRY()
    data = "this used to be Statements";
}

// Statement-> Type ID;
Statement::Statement(Type *type, Node *ID) {
    FUNC_ENTRY()
    // checking if the variable name already exists in the scope
    if (IDExists(ID->value)) {
        output::errorDef(yylineno, ID->value);
        exit(0);
    }
    //avoiding malloc statements
    vector<pair<int, BranchLabelIndex>> tmpList1;
    vector<pair<int, BranchLabelIndex>> tmpList2;
    this->breakList = tmpList1;
    this->continueList = tmpList2;

    // insert to current scope
    int offset = offsetStack.back()++;
    auto new_entry = shared_ptr<SymbolEntry>(new SymbolEntry(ID->value, type->value, offset));
    tableStack.back()->symbols.emplace_back(new_entry);

    //Writing LLVM code
    this->data = "type id";
    this->reg = pool.getReg();
    string expType = getLLVMType(type->value);
    buffer.emit(
            "%" + this->reg + " = add " + expType + " 0,0";
            );
    string ptr = pool.getReg();
    //%reg= add i8 reg, reg
    buffer.emit(
            "%" + ptr + " = getelementptr [50 x i32], [50 x i32]*  %stack, i32 0, i32 " + to_string(offset);
            );
    string dataReg = reg;
    if(expType != "i32"){
        dataReg = pool.getReg();
        //%reg = zext i8 reg to i32
        buffer.emit(
                "%" + dataReg + " = zext " + expType + " %" + reg + " to i32"
        );
    }
    buffer.emit("store i32 %" + dataReg + ", i32* %" + ptr);

}


// Statement-> Type ID = Exp;
Statement::Statement(Type *type, Node *ID, Exp *exp) {
    FUNC_ENTRY()
    DEBUG(cout<<"left_type:"<<type->value<<" right_type"<<exp->type<<endl;)
    // checking if the variable name already exists in the scope
    if (IDExists(ID->value)) {
        output::errorDef(yylineno, ID->value);
        exit(0);
    }
    // checking that types match
    if (type->value.compare(exp->type) != 0) {
        if(!(type->value.compare("INT") == 0 && exp->type.compare("BYTE") == 0)){
            output::errorMismatch(yylineno);
            exit(0);
        }
    }
    this->data = exp->value;
    // inserting to current scope
    int offset = offsetStack.back()++;
    auto new_entry = shared_ptr<SymbolEntry>(new SymbolEntry(ID->value, type->value, offset));
    tableStack.back()->symbols.emplace_back(new_entry);

    //Writing LLVM code
    this->reg = pool.getReg();
    string expType = getLLVMType();
    string dataReg = exp->reg;
    if(type->value.compare("INT") == 0 && exp->type.compare("BYTE") == 0){
        //%reg = zext i8 %reg to i32
        dataReg = pool.getReg();
        buffer.emit("%" + dataReg + " = zext i8 %" + exp->reg + " to i32");
    }

    buffer.emit(
            "%" + this->reg + " = add" + expType + " 0,%" + dataReg
            );
    string ptrReg = pool.getReg();
    buffer.emit(
            "%" + this->reg + " = add " + expType + " 0,%" + dataReg
            );
    dataReg = reg;
    if(expType != "i32"){
        dataReg = pool.getReg();
        //%reg = zext i8 reg to i32
        buffer.emit(
                "%" + dataReg + " = zext " + expType + " %" + reg + " to i32"
        );
    }
    buffer.emit("store i32 %" + dataReg + ", i32* %" + ptr);
}

string emitCodeToBuffer(string data, string type, int offset){
    string reg = pool.getReg();
    string dataReg = data;
    string argType = getLLVMType(type);
    if(argType != "i32"){
        dataReg = pool.getReg();
        //%reg = zext i8 reg to i32
        buffer.emit(
                "%" + dataReg + " = zext " + argType + " %" + data + " to i32"
        );
    }
    buffer.emit(
            "%" + reg + " = add i32 0,%" + dataReg
            );
    string ptrReg = pool.getReg();
    if(offset >= 0){
        buffer.emit(
                "%" + ptrReg + " = getelementor [50 x i32], [50 x i32]* %stack, i32 0," +
                "i32 " + to_string(offset)
                );

    } else if(offset < 0 && currentFuncArgs > 0){
        buffer.emit(
                "%" + ptr +
                " = getelementptr [ " + to_string(currentFuncArgs) + "x i32]," +
                " [" + to_string(currentFuncArgs) + " x i32]* %args," +
                " i32 0," +
                " i32 " + to_string(currentFuncArgs + offset)
                );
    } else{
        cout<<"Not suppose get here, currentFuncArgs = 0 and offset < 0"<<endl;
    }

    buffer.emit("store i32 %" + reg + ", i32* %" + ptr);
    return reg;
    //%ptr = getelementptr inbounds[10 x i32]* %args, i32 0, i32 0
    //store i32 %t3, i32* %ptr
}

// Statement-> ID = EXP;
Statement::Statement(Node *ID, Exp *exp) {
    FUNC_ENTRY()
    //avoid malloc statments
    vector<pair<int, BranchLabelIndex>> tmpList1;
    vector<pair<int, BranchLabelIndex>> tmpList2;
    this->breakList = tmpList1;
    this->continueList = tmpList2;

    // checking if the ID is in scope and type fits exp type
    for (int i = tableStack.size()-1; i > 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name.compare(ID->value) == 0) {
                if (tableStack[i]->symbols[j]->type.size() == 1) { // checking ID isn't a func name
                    if ((tableStack[i]->symbols[j]->type[0] == "INT" && exp->type == "BYTE") ||
                         tableStack[i]->symbols[j]->type[0] == exp->type) {
                        this->data = exp->value;
                        this->value = tableStack[i]->symbols[j]->type[0];
                        this->instruction = exp->instruction;
                        this->reg = emitCodeToBuffer(exp->reg, exp->type, tableStack[i]->symbols[j]->offset);
                        return;
                    }
                } else {
                    output::errorUndef(yylineno, ID->value);
                    exit(0);
                }
            }
        }
    }
    // ID not found
    output::errorUndef(yylineno, ID->value);
    exit(0);
}

// Statement-> Call;
Statement::Statement(Call *call) {
    FUNC_ENTRY()
    this->data = "this was a call for a function";
}

// Statement-> return;
Statement::Statement(RetType *retType) {
    FUNC_ENTRY()
    vector<pair<int, BranchLabelIndex>> tmpList1;
    vector<pair<int, BranchLabelIndex>> tmpList2;
    this->breakList = tmpList1;
    this->continueList = tmpList2;

    for (int i = tableStack.size() -1; i >= 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == currentFunc){
                int size = tableStack[i]->symbols[j]->type.size();
                if (tableStack[i]->symbols[j]->type.back().compare("VOID") == 0) {
                    this->data = "ret val of void";
                    buffer.emit("ret void");
                    return;
                } else {
                    output::errorMismatch(yylineno);
                    exit(0);
                }
            }
        }
    }
    output::errorUndef(yylineno, "zazi bazazi");
    exit(0);
}

// Statement-> return Exp;
Statement::Statement(Exp *exp) {
    FUNC_ENTRY()
    vector<pair<int, BranchLabelIndex>> tmpList1;
    vector<pair<int, BranchLabelIndex>> tmpList2;
    this->breakList = tmpList1;
    this->continueList = tmpList2;

    if (exp->type.compare("VOID") == 0) {
        output::errorMismatch(yylineno);
        exit(0);
    }
    for (int i = tableStack.size() -1; i >= 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == currentFunc){
                int size = tableStack[i]->symbols[j]->type.size();
                if (tableStack[i]->symbols[j]->type[size - 1] == exp->type) {
                    this->data = exp->value;
                    string LLVMRetType = getLLVMType(exp->type);
                    buffer.emit("ret " + LLVMRetType + " %"+exp->reg);
                    return;
                } else if (exp->type == "BYTE" && tableStack[i]->symbols[j]->type[size - 1] == "INT") {
                    this->data = exp->value;
                    string dataReg = pool.getReg();
                    buffer.emit(
                            "%" + dataReg + " = zext i8 %" + exp->reg + " to i32"
                            );
                    buffer.emit("ret i32 %" + dataReg);
                    return;
                } else {
                    output::errorMismatch(yylineno);
                    exit(0);
                }
            }
        }
    }
    output::errorUndef(yylineno, "zazi bazazi2");
    exit(0);
}


// if, if else, while
Statement::Statement(string str, Exp *exp) {
    FUNC_ENTRY()
    if (exp->type != "BOOL") {
        output::errorMismatch(yylineno);
        exit(0);
    }
    this->data = "this was an if/ if else/ while";
}

// break, continue
Statement::Statement(Node *terminal) {
    FUNC_ENTRY()
    DEBUG(cout<<"value:"<<terminal->value<<" ,loopCount:"<<loopCount<<" insideCase:"<<insideCase<<endl;)
    if (terminal->value == "break") {
        if(loopCount <= 0 && insideCase<=0){
            output::errorUnexpectedBreak(yylineno);
            exit(0);
        }
    }
    else if(loopCount <= 0){
        output::errorUnexpectedContinue(yylineno);
        exit(0);
    }
    this->data = "this was a break/ continue";
    FUNC_EXIT()
}

// switch (Exp) {CaseList}
Statement::Statement(Exp *exp, CaseList *caseList) {
    FUNC_ENTRY()
    this->data = "this was switch case";
}

/************************ SWITCH CASE ************************/
// CaseDecl-> Case NUM: Statements
CaseDecl::CaseDecl(Node* num, Statements *statements) {
    FUNC_ENTRY()
}

// CaseList-> CaseDecl, CaseList
CaseList::CaseList(CaseDecl *caseDecl, CaseList *caseList) {
    FUNC_ENTRY()
}

// CaseList-> Default: Statements
CaseList::CaseList(Statements *statements) {
    FUNC_ENTRY()
}

Funcs::Funcs() {
    FUNC_ENTRY()
    if (strcmp(yytext, "") != 0) {
        output::errorSyn(yylineno);
        exit(0);
    }
}

Program::Program() {
    FUNC_ENTRY()
    shared_ptr <SymbolTable> global = shared_ptr<SymbolTable>(new SymbolTable);
    const vector <string> temp = {"STRING", "VOID"};
    auto print = shared_ptr<SymbolEntry>(new SymbolEntry("print", temp, 0));
    const vector <string> temp2 = {"INT", "VOID"};//to do arg of byte also
    auto printi = shared_ptr<SymbolEntry>(new SymbolEntry("printi", temp2, 0));
    global->symbols.emplace_back(print);
    global->symbols.emplace_back(printi);
    tableStack.emplace_back(global);
    offsetStack.emplace_back(0);
}

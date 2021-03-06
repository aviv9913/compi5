//
// Created by Aviv on 17/05/2021.
//

#include "IRManager.hpp"

extern char *yytext;

/// Global variables
string currentFunc;
int currentFuncArgs;
int loopCount = 0;
int insideCase = 0;
int caseNum = 0;
int maxCase = 0;

/// Table Stack
vector<shared_ptr<SymbolTable>> tableStack;
vector<int> offsetStack;

/// Code Buffer
//CodeBuffer &buffer = CodeBuffer::instance();
IRManager &llvm = IRManager::instance();

void printMsgToErr(string s){
    cerr<<s<<endl;
}

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
    insideCase++;
    maxCase++;
    caseNum = maxCase;
}

void decCase() {
    insideCase--;
    if(insideCase > 0){
        caseNum--;
    }
}

void incLoop() {
    loopCount++;
}
//DONE: add M, N, P to parser
void decLoop(N *first, P *second, Statement *st) {
    loopCount--;

    int new_loc = emitUnconditional();
    string new_label = genLabel();
    bpatch(makeList(bp_pair(first->loc, FIRST)), first->instruction);
    bpatch(makeList(bp_pair(second->loc, FIRST)), second->instruction);
    bpatch(makeList(bp_pair(second->loc, SECOND)), new_label);
    bpatch(makeList(bp_pair(new_loc, FIRST)), first->instruction);

    if (st->breakList.size() != 0) {
        bpatch(st->breakList, new_label);
    }
    if (st->continueList.size() != 0) {
        bpatch(st->continueList, first->instruction);
    }
}

void openScope(){
    FUNC_ENTRY()
    auto newScope = shared_ptr<SymbolTable>(new SymbolTable);
    tableStack.emplace_back(newScope);
    offsetStack.push_back(offsetStack.back());
}

void closeScope(){
    FUNC_ENTRY()
    //output::endScope();
    auto scope_table = tableStack.back();
    for (auto i : scope_table->symbols) {//printing all the variables and functions
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
}

void endProgram() {
    FUNC_ENTRY()
    auto global = tableStack.front()->symbols;
    bool mainFound = false;
    for (int i = 0; i < global.size(); ++i) {
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
    printGlobalBuffer();
    printCodeBuffer();
}

void endCurrentFunc(RetType *retType) {
    FUNC_ENTRY()
    if (retType->value == "VOID") {
        emit("ret void");
    } else {
        string type = getLLVMType(retType->value);
        emit("ret " + type + " 0");
    }
    emit("}");
    currentFunc = "";
    currentFuncArgs = 0;
}

void enterArgsToStackTable(Formals *fm) {
    FUNC_ENTRY()
    for (int i = 0; i < fm->formals.size(); ++i) {
        auto temp = shared_ptr<SymbolEntry>(new SymbolEntry(fm->formals[i]->value, fm->formals[i]->type,-i-1));
        tableStack.back()->symbols.push_back(temp);
    }
}

bool IDExists(string id) {
    for (int i = tableStack.size()-1 ; i>=0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name.compare(id) == 0) {
                return true;
            }
        }
    }
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

bool isSigned(string left_type, string right_type = "") {
    if (left_type == "INT" || (!right_type.empty() && right_type == "INT")) {
        return false;
    }
    return true;
}

void ifBPatch(M *label, Exp *exp) {
    int loc = emitUnconditional();
    string end_l = genLabel();
    bpatch(exp->trueList, label->instruction);
    bpatch(exp->falseList, end_l);
    bpatch(makeList(bp_pair(loc, FIRST)), end_l);
}

void ifElseBPatch(M* m_label, N* n_label, Exp *exp) {
    int loc2 = emitUnconditional();
    string end_l = genLabel();
    bpatch(exp->trueList, m_label->instruction);
    bpatch(exp->falseList, n_label->instruction);
    bpatch(makeList(bp_pair(n_label->loc, FIRST)), end_l);
    bpatch(makeList(bp_pair(loc2, FIRST)), end_l);
}

/************************ cast to P ************************/
Node* castExpToP(Exp *left) {
    Node *temp = new P(left);
    return temp;
}

/************************ M, N, P ************************/
M::M() {
    FUNC_ENTRY()
    this->instruction = genLabel();
}

N::N() {
    FUNC_ENTRY()
    this->loc = emitUnconditional();
    this->instruction = genLabel();
}

P::P(Exp *left) {
    FUNC_ENTRY()
    this->loc = emitConditionFromResult(left->reg);
    this->instruction = genLabel();
    DEBUG(printMsgToErr("@@@@@@@@@@@@@@@@ left->reg: " + left->reg);)
    DEBUG(printMsgToErr("############### this->instruction:" + this->instruction);)
    DEBUG(printMsgToErr("@@@@@@@@@@@@ this->loc: " + to_string(this->loc));)
}

Node::Node(string str) {
    DEBUG(cerr<<str<<"|"<<endl;)
    if (str == "void") {
        value = "VOID";
    } else if (str == "bool") {
        value = "BOOL";
    } else if (str == "int") {
        value = "INT";
    } else if (str == "byte") {
        value = "BYTE";
    } else if(str == "OpenNewCase"){
        value = str;
        this->instruction = DeclareCaseLabel();
    } else {
        value = str;
    }

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
                this->reg = emitCall(getLLVMType(this->value), ID->value, "()");
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
        cerr<<exp.value<<","<<exp.type<<endl;
        }
    )
    auto global_scope = tableStack.front()->symbols;
    for (auto i:global_scope) {
        if (i->name == ID->value) {
            if (i->type.size() == 1) {
                output::errorUndefFunc(yylineno, ID->value);
                exit(0);
            }
            string args = "(";
            if (i->type.size() == 1 + paramList->expList.size()) {
                for (int j = 0; j < paramList->expList.size(); ++j) {
                    if (paramList->expList[j].type == "BYTE" && i->type[j] == "INT") {
                        string reg = getReg();
                        emitZext(reg, paramList->expList[j].reg, getLLVMType(paramList->expList[j].type));
                        args += getLLVMType("INT") + " " + reg + ",";
                        continue;
                    }
                    args += getLLVMType(i->type[j]) + " " + paramList->expList[j].reg + ",";
                    if (paramList->expList[j].type != i->type[j]) {
                        i->type.pop_back(); // removing the return type of the function
                        output::errorPrototypeMismatch(yylineno, i->name, i->type);
                        exit(0);
                    }
                }
                args.back() = ')';
                this->value = i->type.back();
                this->reg = emitCall(getLLVMType(this->value), ID->value, args);
                int loc = emitUnconditional();
                this->instruction = genLabel();
                bpatch(makeList(bp_pair(loc, FIRST)), this->instruction);
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
    DEBUG(printMsgToErr(str);)
    this->type = "";
    this->boolValue = false;
    vector<bp_pair> false_list;
    vector<bp_pair> true_list;
    this->falseList = false_list;
    this->trueList = true_list;

    if (str.compare("NUM") == 0) {
        this->type = "INT";
        this->reg = llvm.assignToReg(terminal->value, false);
    }
    else if (str.compare("B") == 0) {
        if (stoi(terminal->value) > 255) {
            output::errorByteTooLarge(yylineno, terminal->value);
            exit(0);
        }
        this->type = "BYTE";
        this->reg = llvm.assignToReg(terminal->value, true);
    }
    else if (str.compare("STRING") == 0) {
        this->type = "STRING";
        this->reg = llvm.addGlobalString(terminal->value);
    }
    else if (str.compare("TRUE") == 0) {
        this->boolValue = true;
        this->type = "BOOL";
        this->reg = llvm.assignBoolToReg("1");
    }
    else if(str == "SWITCH") {
        this->type = "INT";
        this->reg = llvm.assignToReg(terminal->value, false);
    }
    else if(str == "CASE_NUM"){
        this->type = "INT";
    }
    else {
        this->boolValue = false;
        this->type = "BOOL";
        this->reg = llvm.assignBoolToReg("0");
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
    this->falseList = exp->trueList;
    this->trueList = exp->falseList;
    this->reg = llvm.assignBoolToReg(exp->reg);
}
//DONE: update parser
// Exp op Exp
Exp::Exp(Exp *left, Node *op, Exp *right, string str, P *shortC) {
    FUNC_ENTRY()
    this->type = "";
    this->boolValue = false;
    this->falseList = vector<bp_pair>();
    this->trueList = vector<bp_pair>();
    string end_instr;

    DEBUG(printMsgToErr("left:"+left->type+", op:"+op->value+", right:"+right->type+", srting:"+str);)
    // RELOP checking if both exp are numbers
    if ((left->type.compare("INT") == 0 || left->type.compare("BYTE") == 0) &&
        (right->type.compare("INT") == 0 || right->type.compare("BYTE") == 0)) {
        bool isInt = isSigned(left->type, right->type);
        if (str.compare("RELOPL") == 0 || str.compare("RELOPN") == 0) {
            this->type = "BOOL";
            this->reg = llvm.relop(left, right, op->value, isInt);
            if (!right->instruction.empty()) {
                end_instr = right->instruction;
            } else {
                end_instr = left->instruction;
            }
        } // BINOP
        else if (str.compare("ADD") == 0 || str.compare("MUL") == 0) {
            this->type = "BYTE";
            if (left->type.compare("INT") == 0 || right->type.compare("INT") == 0) {
                this->type = "INT";
            }
            vector<string> res = llvm.binop(left, right, op->value, isInt);
            this->reg = res[0];
            end_instr = res[1];
        }
    } // AND, OR
    else if (left->type.compare("BOOL") == 0 &&
            right->type.compare("BOOL") == 0) {
        if (op->value != "and" && op->value != "or" ) {
            output::errorMismatch(yylineno);
            exit(0);
        }
        this->type = "BOOL";
        if (!right->instruction.empty()) {
            this->instruction = right->instruction;
        } else {
            this->instruction = shortC->instruction;
        }
        DEBUG(printMsgToErr("############### p->instruction: " + shortC->instruction);)
        vector<string> bool_res = llvm.boolop(left->reg, right->reg, op->value, this->instruction, shortC);
        this->reg = bool_res[0];
        end_instr = bool_res[1];
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
    if (!end_instr.empty()) {
        this->instruction = end_instr;
    }
    DEBUG(cerr<<"type after calc:"<<this->type<<endl;)
    DEBUG(cerr << "this->reg: " << this->reg << endl;)
}

// (Exp)
Exp::Exp(Exp *exp) {
    FUNC_ENTRY()
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
    this->reg = exp->reg;
    this->instruction = exp->instruction;
    this->trueList = exp->trueList;
    this->falseList = exp->falseList;
}

// ID
Exp::Exp(Node *ID) {
    FUNC_ENTRY()
    if (!IDExists(ID->value)) {
        output::errorUndef(yylineno, ID->value);
        exit(0);
    }

    this->falseList =  vector<bp_pair>();
    this->trueList = vector<bp_pair>();
    for (int i = tableStack.size()-1; i > 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == ID->value) {
                this->value = ID->value;
                this->type = tableStack[i]->symbols[j]->type.back();
                this->boolValue = false;
                this->reg = llvm.loadVar(tableStack[i]->symbols[j]->offset, currentFuncArgs, this->type);
                DEBUG(printMsgToErr("type = " + tableStack[i]->symbols[j]->type.back() + " for id:"+ID->value + " reg:"+this->reg));
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
    this->reg = call->reg;
    this->instruction = call->instruction;
    vector<bp_pair> false_list;
    vector<bp_pair> true_list;
    this->falseList = false_list;
    this->trueList = true_list;
}

// bool check
Exp::Exp(Exp *exp, string str) {
    FUNC_ENTRY()
    DEBUG(cerr<<"bool check"<<endl;)
    if (exp->type != "BOOL") {
        output::errorMismatch(yylineno);
        exit(0);
    }
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
    this->reg = exp->reg;
    this->instruction = exp->instruction;
    int loc = emitConditionFromResult(this->reg);
    this->trueList = makeList(bp_pair(loc, FIRST));
    this->falseList = makeList(bp_pair(loc, SECOND));
}

//switch exp
Exp::Exp(int x, Exp *exp, string stage, N *label) {
    FUNC_ENTRY()
    if (stage == "switch" && (exp->type != "INT"  && exp->type !="BYTE")) {
        output::errorMismatch(yylineno);
        exit(0);
    }
    if(label != nullptr){
        bpatch(makeList(bp_pair(label->loc, FIRST)),label->instruction);
    }
    this->value = exp->value;
    this->type = exp->type;
    this->boolValue = exp->boolValue;
    this->reg = exp->reg;
    this->instruction = exp->instruction;
    this->trueList = vector<bp_pair>();
    this->falseList = vector<bp_pair>();
    if(stage == "switch"){
        emitBranchLabel("label_switch_" + to_string(caseNum));
    }
    else{
        int location = emitConditionFromResult(this->reg);
        trueList = makeList(bp_pair(location, FIRST));
        falseList = makeList(bp_pair (location, SECOND));
    }
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
    DEBUG(cerr<<"Arguments string: "<<argString<<endl;)
    this->types.emplace_back(retType->value);
    // open new scope for func

    auto func_scope = shared_ptr<SymbolEntry>(new SymbolEntry(this->value, this->types, 0));
    tableStack[0]->symbols.push_back(func_scope);
    currentFunc = this->value;
    currentFuncArgs = args->formals.size();
    string retTypeString = getLLVMType(retType->value);
    emit(
      "define " + retTypeString + " @"+ this->value + argString + " {"
    );
    emit("%stack = alloca [50 x i32]");
    emit("%args = alloca [" + to_string(args->formals.size()) + " x i32]");
    for(int i=0; i<args->formals.size(); ++i){
        string ptrReg = getReg();
        llvm.emitGetElementPtr(ptrReg, "%args", args->formals.size(), currentFuncArgs - i - 1);
        string dataReg = to_string(i);
        string argType = getLLVMType(args->formals[i]->type);
        if(argType != "i32"){
            dataReg = getReg();
            emitZext(dataReg, to_string(i), argType);
        }
        emitStore(dataReg, ptrReg);
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
// Statement-> (Statements)
Statement::Statement(Statements *sts) {
    FUNC_ENTRY()
//    vector<pair<int, BranchLabelIndex>> tmpList1;
//    vector<pair<int, BranchLabelIndex>> tmpList2;
//    this->breakList = tmpList1;
//    this->continueList = tmpList2;
    data = "this used to be Statements";
    this->breakList = vector<bp_pair>(sts->breakList);
    this->continueList = vector<bp_pair>(sts->continueList);
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
    string expType = getLLVMType(type->value);
    this->reg = llvm.assignToReg("0", expType);
    string ptrReg = getReg();
    llvm.emitGetElementPtr(ptrReg, "%stack", 50, offset);
    string dataReg = reg;
    if(expType != "i32"){
        dataReg = getReg();
        emitZext(dataReg, reg, expType);
    }
    emitStore(dataReg, ptrReg);
}


// Statement-> Type ID = Exp;
Statement::Statement(Type *type, Node *ID, Exp *exp) {
    FUNC_ENTRY()
    DEBUG(cerr<<"left_type:"<<type->value<<" right_type:"<<exp->type<<endl;)
    // checking if the variable name already exists in the scope
    if (IDExists(ID->value)) {
        output::errorDef(yylineno, ID->value);
        exit(0);
    }
    // checking that types match
    if (type->value != exp->type) {
        if(!(type->value == "INT" && exp->type == "BYTE")){
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
//    this->reg = getReg();
    string expType = getLLVMType(exp->type);
    string dataReg = exp->reg;
    if(type->value == "INT" && exp->type == "BYTE"){
        //%reg = zext i8 %reg to i32
        dataReg = getReg();
        emitZext(dataReg, exp->reg, expType);
    }
    this->reg = llvm.assignToReg(dataReg, expType);
    string ptrReg = getReg();
    llvm.emitGetElementPtr(ptrReg, "%stack", 50, offset);
    dataReg = this->reg;
    if(expType != "i32"){
        dataReg = getReg();
        emitZext(dataReg, reg, expType);
    }
    emitStore(dataReg, ptrReg);
}

string emitCodeToBuffer(string data, string type, int offset){
    string reg = getReg();
    string dataReg = data;
    string argType = getLLVMType(type);
    if(argType != "i32"){
        dataReg = getReg();
//        dataReg = std::to_string(emitZext(dataReg, data, argType));
        emitZext(dataReg, data, argType);
    }
    DEBUG(cerr << "******************* dataReg: " << dataReg << endl;)
    llvm.assignToReg(dataReg, false, reg=reg);
    string ptrReg = getReg();
    if(offset >= 0){
        llvm.emitGetElementPtr(ptrReg, "%stack", 50, offset);

    } else if(offset < 0 && currentFuncArgs > 0){
        llvm.emitGetElementPtr(ptrReg, "%args", currentFuncArgs, currentFuncArgs+offset);

    } else{
        cout<<"Not suppose get here, currentFuncArgs = 0 and offset < 0"<<endl;
    }

    emitStore(reg,ptrReg);
    return reg;
}

// Statement-> ID = EXP;
Statement::Statement(Node *ID, Exp *exp) {
    FUNC_ENTRY()
    //avoid malloc statments
    if (!IDExists(ID->value)) {
        output::errorUndef(yylineno, ID->value);
        exit(0);
    }
    this->breakList = vector<bp_pair>();
    this->continueList = vector<bp_pair>();
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
                        DEBUG(cerr << "exp: reg=" << exp->reg << " type=" << exp->type << endl;)
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
    vector<pair<int, BranchLabelIndex>> listBreak;
    this->breakList = listBreak;
    vector<pair<int, BranchLabelIndex>> listContinue;
    this->continueList = listContinue;
    data = "this was a call for a function";
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
                    emit("ret void");
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
        exit(1);
    }
    for (int i = tableStack.size() -1; i >= 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == currentFunc){
                int size = tableStack[i]->symbols[j]->type.size();
                if (tableStack[i]->symbols[j]->type[size - 1] == exp->type) {
                    this->data = exp->value;
                    string LLVMRetType = getLLVMType(exp->type);
                    emit("ret " + LLVMRetType + " "+exp->reg);
                    return;
                } else if (exp->type == "BYTE" && tableStack[i]->symbols[j]->type[size - 1] == "INT") {
                    this->data = exp->value;
                    string dataReg = getReg();
                    emitZext(dataReg, exp->reg, getLLVMType(exp->type));
                    emit("ret i32 " + dataReg);
                    return;
                } else {
                    output::errorMismatch(yylineno);
                    exit(1);
                }
            }
        }
    }
    output::errorUndef(yylineno, "zazi bazazi2");
    exit(1);
}


// if, if else, while
Statement::Statement(string str, Exp *exp, Statement *st) {
    FUNC_ENTRY()
    if (exp->type != "BOOL") {
        output::errorMismatch(yylineno);
        exit(1);
    }
    if (st != nullptr) {
        this->continueList = st->continueList;
        this->breakList = st->breakList;
    }
    else{
        this->breakList = vector<bp_pair>();
        this->continueList = vector<bp_pair>();
    }
    this->data = "this was an if/ if else/ while";
}

Statement *addElseStatement(Statement *stIf, Statement *stElse) {
    stIf->breakList = merge(stIf->breakList, stElse->breakList);
    stIf->continueList = merge(stIf->continueList, stElse->continueList);
    return stIf;
}

// break, continue
Statement::Statement(Node *terminal) {
    FUNC_ENTRY()
    string terminalValue = terminal->value;
    vector<pair<int, BranchLabelIndex>> tmpList1;
    vector<pair<int, BranchLabelIndex>> tmpList2;
    this->breakList = tmpList1;
    this->continueList = tmpList2;

    if(loopCount <= 0 && insideCase<=0){
        if(terminal->value == "break"){
            output::errorUnexpectedBreak(yylineno);
            exit(1);
        }
        else if(terminal->value == "continue"){
            output::errorUnexpectedContinue(yylineno);
            exit(1);
        }
    }
    else if(loopCount > 0){
        int location = emitUnconditional();
        if(terminal->value == "break"){
            breakList = makeList(bp_pair(location, FIRST));
        }
        else {
            continueList = makeList(bp_pair(location, FIRST));
        }
    }
    else if(insideCase > 0){
        int location = emitUnconditional();
        if(terminal->value == "break"){
            breakList = makeList(bp_pair(location, FIRST));
        }
        else {
            output::errorUnexpectedContinue(yylineno);
            exit(1);
        }
    }
    else if (terminal->value == "continue" && insideCase > 0){
        output::errorUnexpectedContinue(yylineno);
        exit(1);
    }
    //DONE: implement break inside case scenario
    this->data = "this was a break/continue";
}

// switch (Exp) {CaseList}
Statement::Statement(Exp *exp, CaseList *caseList) {
    FUNC_ENTRY()
    if(exp->type != "INT" && exp->type != "BYTE"){
        output::errorMismatch(yylineno);
        exit(1);
    }
    for(auto &c : caseList->cases){
        if(c->value != "INT" && c->value != "BYTE"){
            output::errorMismatch(yylineno);
            DEBUG(cerr<<"case number has wrong type, type ="<<c->value<<endl;)
            exit(1);
        }
    }

    this->data = "this was switch case";

    emit("label_switch_" + to_string(caseNum) + ":");
    int defaultCaseIndex = 0;
    string finalLabel = "label_switch_" + to_string(caseNum) + "_final";
    for (int i = caseList->cases.size()-1; i>=0 ; --i){
        shared_ptr<CaseDecl> currCase = caseList->cases[i];
        if(currCase->reg == "default"){
            defaultCaseIndex = i;
            continue;
        }
        string tempReg = llvm.assignToReg(to_string(currCase->num), false);
        string compareReg = llvm.relop(exp->reg, tempReg, "==");
        string falseCaseLabel = "label_switch_" + to_string(caseNum) + "_case_" + to_string(i);
        emit("br i1 " + compareReg + ", label %"+ currCase->instruction + ", label %" + falseCaseLabel);
        emit(falseCaseLabel + ":");
        if(!currCase->breakList.empty()){
            bpatch(currCase->breakList, finalLabel);
        }
        if(caseList->cases.size() == 1){
            emitBranchLabel(finalLabel);
        }
    }

    emitBranchLabel(caseList->cases[defaultCaseIndex]->instruction);
    bpatch(caseList->breakList, finalLabel);
    emit(finalLabel + ":");
    this->breakList = vector<bp_pair>();
    this->continueList = merge(vector<bp_pair>(), caseList->continueList);
    for (auto &c : caseList->cases){
        this->continueList = merge(this->continueList, c->continueList);
    }
}

Statements::Statements(Statement *st) {
    FUNC_ENTRY()
    this->breakList = st->breakList;
    this->continueList = st->continueList;
    FUNC_EXIT()
}

Statements::Statements(Statements *sts, Statement *st) {
    FUNC_ENTRY()
    this->breakList = merge(sts->breakList, st->breakList);
    this->continueList = merge(sts->continueList, st->continueList);
}

/************************ SWITCH CASE ************************/
string DeclareCaseLabel(){
    FUNC_ENTRY()
    int location = emitUnconditional();
    string label = genLabel();
    bpatch(makeList(bp_pair(location, FIRST)), label);
    return label;
}



// CaseDecl-> Case NUM: Statements
CaseDecl::CaseDecl(Exp* num, Statements *statements, Node* label) {
    FUNC_ENTRY()
    if(num->type != "INT" && num->type != "BYTE"){
        output::errorMismatch(yylineno);
        DEBUG(cerr<<"case number is not int or byte, num->type="<<num->type<<endl;)
        exit(1);
    }
    this->instruction = label->instruction;
    this->reg = num->reg;
    this->value = num->type;
    this->num = stoi(num->value);
    this->breakList = statements->breakList;
    this->continueList = statements->continueList;
}

// CaseList-> CaseDecl, CaseList
CaseList::CaseList(CaseDecl *caseDecl, CaseList *caseList) {
    FUNC_ENTRY()
    this->cases = vector<shared_ptr<CaseDecl>>(caseList->cases);
    this->cases.emplace_back(caseDecl);
    this->breakList = merge(caseList->breakList, caseDecl->breakList);
    this->continueList = merge(caseList->continueList, caseDecl->breakList);
    this->value = "this was a case list";
}

CaseList::CaseList(CaseDecl *caseDecl) {
    FUNC_ENTRY()
    this->cases.emplace_back(caseDecl);
    this->breakList = caseDecl->breakList;
    this->continueList = caseDecl->breakList;
    this->value = "this was a case list";
}

// CaseList-> Default: Statements
CaseList::CaseList(Statements *statements, N *label) {
    FUNC_ENTRY()
    CaseDecl *tempCase = new CaseDecl();
    tempCase->value = "INT";
    tempCase->reg = "default";
    tempCase->instruction = label->instruction;

    bpatch(makeList(bp_pair(label->loc, FIRST)), label->instruction);
    this->cases.emplace_back(tempCase);
    int new_location = emitUnconditional();
    this->breakList = merge(statements->breakList, makeList(bp_pair(new_location, FIRST)));
    this->continueList = statements->continueList;
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
    //DONE: make sure all the default stuff get printed from addExitAndPrintFunctions();
}

//
// Created by Aviv on 17/05/2021.
//

#include "library.h"
#include <cstring>
extern char *yytext;


int loopCount = 0;
int insideCase=0;
string currentFunc;
vector<shared_ptr<SymbolTable>> tableStack;
vector<int> offsetStack;

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

void decLoop() {
    loopCount--;
}

void endCurrentFunc() {
    currentFunc = "";
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
    for (auto i:scope_table->symbols) {//printing all the variables(not enumDef) and functions
        if (i->type.size() == 1) { //variable
            output::printID(i->name, i->offset, i->type[0]);
        } else {
            auto retVal = i->type.back();
            i->type.pop_back();
            if (i->type.front() == "VOID") {
                i->type.pop_back();
            }
            //functions
            output::printID(i->name, i->offset, output::makeFunctionType(retVal, i->type));
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
    if (str.compare("NUM") == 0) {
        this->type = "INT";
    } else if (str.compare("B") == 0) {
        if (stoi(terminal->value) > 255) {
            output::errorByteTooLarge(yylineno, terminal->value);
            exit(0);
        }
        this->type = "BYTE";
    } else if (str.compare("STRING") == 0) {
        this->type = "STRING";
    } else if (str.compare("TRUE") == 0) {
        this->boolValue = true;
        this->type = "BOOL";
    } else {
        this->boolValue = false;
        this->type = "BOOL";
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
}

// Exp op Exp
Exp::Exp(Exp *left, Node *op, Exp *right, string str) {
    FUNC_ENTRY()
    this->type = "";
    this->boolValue = false;
    DEBUG(cout<<"left:"<<left->type<<", op:"<<op->value<<", right:"<<right->type<<", srting:"<<str<<endl;)
    // RELOP checking if both exp are numbers
    if ((left->type.compare("INT") == 0 || left->type.compare("BYTE") == 0) &&
        (right->type.compare("INT") == 0 || right->type.compare("BYTE") == 0)) {
        if (str.compare("RELOPL") == 0 || str.compare("RELOPN") == 0) {
            this->type = "BOOL";
        } // BINOP
        else if (str.compare("ADD") == 0 || str.compare("MUL") == 0) {
            this->type = "INT";
            if (left->type.compare("BYTE") == 0 || right->type.compare("BYTE")) {
                this->type = "BYTE"; //
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
        if (args->formals[i]->value.compare(ID->value) == 0) {
            output::errorDef(yylineno, args->formals[i]->value);
            exit(0);
        }
        // TODO: maybe move to Formals class
        // checking that 2 params don't have the same name
        for (int j = i + 1; j < args->formals.size(); ++j) {
            if (args->formals[i]->value.compare(args->formals[j]->value) == 0) {
                output::errorDef(yylineno, args->formals[i]->value);
                exit(0);
            }
        }
    }
    this->value = ID->value;
    if (args->formals.size() != 0) {
        for (int i = 0; i < args->formals.size(); ++i) {
            this->types.push_back(args->formals[i]->type);
        }
    }
    else{
        this->types.emplace_back("VOID");
    }
    this->types.emplace_back(retType->value);
    // open new scope for func

    auto func_scope = shared_ptr<SymbolEntry>(new SymbolEntry(this->value, this->types, 0));
    tableStack[0]->symbols.push_back(func_scope);
    currentFunc = this->value;
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
    this->data = "type id";
    // insert to current scope
    int offset = offsetStack.back()++;
    auto new_entry = shared_ptr<SymbolEntry>(new SymbolEntry(ID->value, type->value, offset));
    tableStack.back()->symbols.emplace_back(new_entry);
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
}

// Statement-> ID = EXP;
Statement::Statement(Node *ID, Exp *exp) {
    FUNC_ENTRY()
    // checking if the ID is in scope and type fits exp type
    for (int i = tableStack.size()-1; i > 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name.compare(ID->value) == 0) {
                if (tableStack[i]->symbols[j]->type.size() == 1) { // checking ID isn't a func name
                    if ((tableStack[i]->symbols[j]->type[0] == "INT" && exp->type == "BYTE") ||
                         tableStack[i]->symbols[j]->type[0] == exp->type) {
                        this->data = exp->value;
                        this->value = tableStack[i]->symbols[j]->type[0];
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

// TODO: not sure about this implementation
// Statement-> return;
Statement::Statement(RetType *retType) {
    FUNC_ENTRY()
    DEBUG(cout<<"symbol_stack size:"<<tableStack.size()<<endl;)
    for (int i = tableStack.size() -1; i >= 0; --i) {
        for (int j = 0; j < tableStack[i]->symbols.size(); ++j) {
            if (tableStack[i]->symbols[j]->name == currentFunc){
                int size = tableStack[i]->symbols[j]->type.size();
                DEBUG(cout<<"size:"<<size<<" it has to be >0 "<<endl;)
                DEBUG(if(size>0) cout<<"funcRetType:"<<tableStack[i]->symbols[j]->type.back()<<endl;)
                if (tableStack[i]->symbols[j]->type.back().compare("VOID") == 0) {
                    this->data = "ret val of void";
                    return;
                } else {
                    output::errorMismatch(yylineno);
                    exit(0);
                }
            }
        }
    }
    output::errorUndef(yylineno, "yaba daba doo"); // shouldn't reach this
    exit(0);
}

// Statement-> return Exp;
Statement::Statement(Exp *exp) {
    FUNC_ENTRY()
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
                    return;
                } else if (exp->type == "BYTE" && tableStack[i]->symbols[j]->type[size - 1] == "INT") {
                    this->data = exp->value;
                    return;
                } else {
                    output::errorMismatch(yylineno);
                    exit(0);
                }
            }
        }
    }
    output::errorUndef(yylineno, "yaba daba doo2"); // should not reach this
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

//
// Created by Aviv on 17/05/2021.
//

#ifndef COMPI3_LIBRARY_H
#define COMPI3_LIBRARY_H

#include <cstring>
#include <sstream>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <iostream>
#include "hw3_output.hpp"
#include "bp.hpp"

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;

#define DEBUG(X) X
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#define DEBUG(X)
#endif

extern int yylineno;
using namespace std;

class SymbolEntry{
public:
    string name;
    vector <string> type;
    int offset;

    SymbolEntry(string name, vector<string> type, int offset): name(name), type(type), offset(offset) {}
    SymbolEntry(string name, string type, int offset): name(name), offset(offset) {
        this->type.emplace_back(type);
    }
};

class SymbolTable{
public:
    vector<shared_ptr<SymbolEntry>> symbols;
    SymbolTable() = default;
};

// declaring a few classes, all of them inherent from Node
class Node;
class Formals;
class ExpList;
class Statements;
class Statement;
class Exp;
class RetType;
class M;
class N;
class P;

void openScope();
void closeScope();
void endCurrentFunc(RetType *retType);
void decLoop(N *first, P *second, Statement *st) ;
void incLoop();
void endProgram();
void incCase();
void decCase();

string getLLVMType(string type);
Node* castExpToP(Exp *left);
string DeclareCaseLabel();

class Node {
public:
    string value;
    string reg;
    string instruction;
    Node(string str);

    Node() {
        value = "";
        instruction = "";
    }

    virtual ~Node() {};
};

#define YYSTYPE Node*

// New Classes
class M : public Node {
public:
    M();
};

class N : public Node {
public:
    int loc;
    N();
};

class P : public Node {
public:
    int loc;
    P(Exp *left);
};

// Type
class Type : public Node {
public:
    Type(Node *type) : Node(type->value) {};
};

// Call
class Call : public Node {
public:
    // ID()
    Call(Node *ID);

    // ID(ExpList)
    Call(Node *ID, ExpList *expList);
};

// Exp
class Exp : public Node {
public:
    string type;
    bool boolValue;
    vector <pair<int,BranchLabelIndex>> trueList;
    vector <pair<int,BranchLabelIndex>> falseList;
    string startLabel;
    int loc;

    // NUM, NUM B, STRING, TRUE, FALSE
    Exp(Node *terminal, string str);

    // !Exp
    Exp(Node *Not, Exp *exp);

    //switch exp
    Exp(int x, Exp *exp, string stage, N *label = nullptr);

    // BINOP, AND, OR, RELOP
    Exp(Exp *left, Node *op, Exp *right, string str, P *shortC = nullptr);

    // (Exp) also check missing string
    Exp(Exp *exp);

    // ID
    Exp(Node *ID);

    // Call
    Exp(Call *call);

    // Bool check
    Exp(Exp* exp, string str);
};

// ExpList
class ExpList : public Node {
public:
    vector<Exp> expList;
    // Exp
    ExpList(Exp *exp);

    // Exp, ExpList
    ExpList(Exp *exp, ExpList *paramList);
};

// Program
class Program : public Node {
public:
    Program();
};


// Funcs
class Funcs : public Node {
public:
    Funcs();
};

// RetType
class RetType : public Node {
public:
    RetType(Node *type) : Node(type->value) {};
};

// FuncDecl declaring a new func
class FuncDecl : public Node {
public:
    vector<string> types;
    FuncDecl(RetType *retType, Node *ID, Formals *args);
};

// FormalDecl declaring a new variable
class FormalDecl : public Node {
public:
    string type;
    // Type ID
    FormalDecl(Type *type, Node *ID) : Node(ID->value), type(type->value) {}
};

// FormalsList function params list
class FormalsList : public Node {
public:
    // Note: the value of formals list is empty
    vector<FormalDecl*> formals;
    // FormalDecl
    FormalsList(FormalDecl *formal);

    // FormalDecl, FormalsList
    FormalsList(FormalDecl *formal, FormalsList *formalsList);
};

// Formals
class Formals : public Node {
public:
    vector<FormalDecl*> formals;
    // epsilon
    Formals() {};

    // FormalsList
    Formals(FormalsList *formalsList);
};

// CaseDecl
class CaseDecl : public Node {
public:
    int num;
    vector<pair<int, BranchLabelIndex>> breakList;
    vector<pair<int, BranchLabelIndex>> continueList;
    // CASE NUM: Statements
    CaseDecl(Exp* num, Statements *statements, Node* label);
    CaseDecl(){};
};

// CaseList
class CaseList : public Node {
public:
    vector<shared_ptr<CaseDecl>> cases;
    vector<pair<int, BranchLabelIndex>> breakList;
    vector<pair<int, BranchLabelIndex>> continueList;

    // CaseDecl CaseList
    CaseList(CaseDecl *caseDecl, CaseList *caseList);

    CaseList(CaseDecl *caseDecl);

    // DEFAULT: Statements
    CaseList(Statements *statements, N *label);
};

// Statement
class Statement : public Node {
public:
    string data;
    string reg;
    vector<pair<int, BranchLabelIndex>> breakList;
    vector<pair<int, BranchLabelIndex>> continueList;
    // (Statements)
    Statement(Statements *sts);

    // Type ID;
    Statement(Type *type, Node *ID);

    // Type ID = Exp;
    Statement(Type *type, Node *ID, Exp *exp);

    // ID = Exp;
    Statement(Node *ID, Exp *exp);

    // Call;
    Statement(Call *call);

    // RETURN;
    Statement(RetType *retType);

    // RETURN Exp;
    Statement(Exp *exp);

    // IF (Exp) Statement, IF (Exp) Statement ELSE Statement
    // WHILE (Exp) Statement
    Statement(string str, Exp *exp, Statement *st = nullptr);

    // BREAK; CONTINUE;
    Statement(Node *terminal);

    // SWITCH (Exp) {CaseList}
    Statement(Exp *exp, CaseList *caseList);

};

Statement *addElseStatement(Statement* stIf,Statement* stElse);

// Statements
class Statements : public Node {
public:
    vector <pair<int,BranchLabelIndex>> breakList;
    vector <pair<int,BranchLabelIndex>> continueList;
    // Statement
    Statements(Statement *st);

    // Statements Statement
    Statements(Statements *sts, Statement *st);
};

void enterArgsToStackTable(Formals *fm);

void ifBPatch(M *label, Exp *exp);
void ifElseBPatch(M* m_label, N* n_label, Exp *exp);


#endif //COMPI3_LIBRARY_H

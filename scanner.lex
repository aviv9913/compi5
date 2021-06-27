%{
#include <stdio.h>
#include "library.h"
#include "parser.tab.hpp"
%}

%option yylineno
%option noyywrap
%option nounput

num         [0]|([1-9][0-9]*)
id	        [A-Za-z][A-Za-z0-9]*
comment     \/\/[^\r\n]*[\r|\n|\r\n]?
whitespace  ([\t\n\r ])
%x STRINGS

%%
void                               yylval=new Node(yytext);   return VOID;
int                                yylval=new Node(yytext);   return INT;
byte                               yylval=new Node(yytext);   return BYTE;
b                                  yylval=new Node(yytext);   return B;
bool                               yylval=new Node(yytext);   return BOOL;
and                                yylval=new Node(yytext);   return AND;
or                                 yylval=new Node(yytext);   return OR;
not                                yylval=new Node(yytext);   return NOT;
true                               yylval=new Node(yytext);   return TRUE;
false                              yylval=new Node(yytext);   return FALSE;
return                             yylval=new Node(yytext);   return RETURN;
if                                 yylval=new Node(yytext);   return IF;
else                               yylval=new Node(yytext);   return ELSE;
while                              yylval=new Node(yytext);   return WHILE;
break                              yylval=new Node(yytext);   return BREAK;
continue                           yylval=new Node(yytext);   return CONTINUE;
switch                             yylval=new Node(yytext);   return SWITCH;
case                               yylval=new Node(yytext);   return CASE;
default                            yylval=new Node(yytext);   return DEFAULT;
(\:)                               yylval=new Node(yytext);   return COLON;
(\;)                               yylval=new Node(yytext);   return SC;
(\,)                               yylval=new Node(yytext);   return COMMA;
(\()                               yylval=new Node(yytext);   return LPAREN;
(\))                               yylval=new Node(yytext);   return RPAREN;
(\{)                               yylval=new Node(yytext);   return LBRACE;
(\})                               yylval=new Node(yytext);   return RBRACE;
(=)                                yylval=new Node(yytext);   return ASSIGN;
(==)|(!=)                          yylval=new Node(yytext);   return RELOPL;
(<)|(>)|(<=)|(>=)                  yylval=new Node(yytext);   return RELOPN;
\+|\-                              yylval=new Node(yytext);   return ADD;
\*|\/                              yylval=new Node(yytext);   return MUL;
{id}                               yylval=new Node(yytext);   return ID;
{num}                              yylval=new Node(yytext);   return NUM;
(\")                               BEGIN(STRINGS);
<STRINGS><<EOF>>                                     {output::errorLex(yylineno); exit(0);};
<STRINGS>([\x00-\x09\x0b-\x0c\x0e-\x21\x23-\x5b\x5d-\x7f]|((\\)(\\))|((\\)(\"))|((\\)(n))|((\\)(r))|((\\)(t))|((\\)(0))|((\\)x))*(\") {BEGIN(INITIAL);  yylval=new Node(yytext);  return STRING;}
<STRINGS>([^(\")])*((\")?)            {output::errorLex(yylineno); exit(0);};
{whitespace}        ;
{comment}           ;
.                                     {output::errorLex(yylineno); exit(0);};

%%

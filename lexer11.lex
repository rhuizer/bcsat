%{
#include <cstring>
#include "bc.hh"
#include "gate.hh"
#include "parser11.hh"
extern void bcp11_error2(const char *, ...);
%}

%option noyywrap
%option nounput
%option yylineno

DIGIT	[0-9]
EOL	[\r\n]

%%


[ \t]		;
"//".*/{EOL}	;
\n		;
\r		bcp11_lineno++;
"ASSIGN"	return(ASSIGN);
"EQUIV"		return(EQUIVf);
"=="		return(EQUIV);
"IMPLY"		return(IMPLYf);
"=>"            return(IMPLY);
"ITE"		return(ITEf);
"OR"		return(ORf);
"|"		return(OR);
"AND"		return(ANDf);
"&"		return(AND);
"EVEN"		return(EVENf);
"ODD"		return(ODDf);
"^"		return(ODD);
"NOT"		return(NOTf);
"!"		return(NOT);
"("		return(LPAREN);
")"		return(RPAREN);
"["		return(LBRACKET);
"]"		return(RBRACKET);
";"		return(SEMICOLON);
","		return(COMMA);
":="		return(DEF);
"T"		return(TRUE);
"F"		return(FALSE);
[a-zA-Z_][a-zA-Z0-9_\.\']* {bcp11_lval.charptr = strdup(bcp11_text); return(ID); }
\"[^\"]+\"	{bcp11_lval.charptr = strdup(bcp11_text); return(ID); }
[1-9][0-9]*	{bcp11_lval.intval = atoi(bcp11_text); return(NUM); }
0		{bcp11_lval.intval = 0; return(NUM); }
.		bcp11_error2("illegal character '%c'", bcp11_text[0]);
%%


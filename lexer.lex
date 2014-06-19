%{
#include <cstring>
#include "bc.hh"
#include "gate.hh"
#include "parser.hh"
extern void bcp_error2(const char *, ...);
bool _bcp_in_header;

%}
%option noyywrap
%option nounput
%option yylineno

DIGIT	[0-9]
EOL	[\r\n]

%x HEADER

%%
%{
  if(_bcp_in_header) {BEGIN(HEADER); }
%}

<HEADER>{
"BC"{DIGIT}+"."{DIGIT}+/{EOL} {
  if(strncmp(bcp_text+2, "1.0", 3) != 0)
    {
      bcp_error2("illegal version '%s'", bcp_text+2);
    }
  BEGIN(INITIAL);
  _bcp_in_header = false;
}
.*/{EOL} {
  bcp_error2("Invalid header line '%s'", bcp_text);
}
}

[ \t]		;
"//".*/{EOL}	;
\n		;
\r		bcp_lineno++;
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
"~"		return(NOT);
"("		return(LPAREN);
")"		return(RPAREN);
"["		return(LBRACKET);
"]"		return(RBRACKET);
";"		return(SEMICOLON);
","		return(COMMA);
":="		return(DEF);
"T"		return(TRUE);
"F"		return(FALSE);
[a-zA-Z_][a-zA-Z0-9_\.\']* {bcp_lval.charptr = strdup(bcp_text); return(ID); }
\"[^\"]+\"	{bcp_lval.charptr = strdup(bcp_text); return(ID); }
[1-9][0-9]*	{bcp_lval.intval = atoi(bcp_text); return(NUM); }
0		{bcp_lval.intval = 0; return(NUM); }
.		bcp_error2("illegal character '%c'", bcp_text[0]);
%%


%{
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <list>
#include "defs.hh"
#include "bc.hh"
#include "gate.hh"

/*
 * Imported lexer functions
 */
extern int bcp11_lex();
extern void bcp11_error(const char *, ...);

/*
 * Should be initialized by BC::parse
 */
BC *bcp11_circuit;

/*
 * Variables exported to BC::parse
 */
std::list<Gate*> bcp11_assigned_gates;

/*
 * Exported error reporting functions
 */
void bcp11_error(const char * fmt, ...);
void bcp11_error2(const char * fmt, ...);

static void add_variable_definition(char *name);
static void add_gate_definition(char *, Gate *);
%}

%union {
  char *charptr;
  int intval;
  Gate *gate;
  std::list<Gate*>* list;
}

%token LPAREN RPAREN LBRACKET RBRACKET
%token SEMICOLON COMMA DEF ASSIGN
%nonassoc EQUIVf IMPLYf ORf ANDf ODDf EVENf NOTf ITEf
%right IMPLY
%left EQUIV
%left OR ODD
%left AND
%left NOT
%nonassoc <charptr> ID
%token TRUE FALSE
%nonassoc <intval> NUM

%type <gate> formula
%type <list> formula_list
%type <gate> treshold_rule

%start circuit
%%

circuit:
	  gate_def
	| circuit gate_def
	;

gate_def:
	  ID SEMICOLON {add_variable_definition($1); }
	| ID DEF formula SEMICOLON { add_gate_definition($1, $3); }
        | ASSIGN assign_list SEMICOLON
	;

assign_list:
          formula			{bcp11_assigned_gates.push_back($1); }
        | assign_list COMMA formula     {bcp11_assigned_gates.push_back($3); }
        ;

formula:
	  formula IMPLY formula	{$$ = bcp11_circuit->new_OR(bcp11_circuit->new_NOT($1), $3); }
	| formula EQUIV formula	{$$ = bcp11_circuit->new_EQUIV($1, $3); }
	| formula OR formula	{$$ = bcp11_circuit->new_OR($1, $3); }
	| formula AND formula	{$$ = bcp11_circuit->new_AND($1, $3); }
	| formula ODD formula	{$$ = bcp11_circuit->new_ODD($1, $3); }
	| NOT formula		{$$ = bcp11_circuit->new_NOT($2); }
	| LPAREN formula RPAREN	{$$ = $2; }
	| ID
	  {
	    Handle *handle = bcp11_circuit->find_gate($1);
	    if(handle)
	      {
		$$ = handle->get_gate(); free($1);
	      }
	    else
	      {
		Gate *new_gate = bcp11_circuit->new_UNDEF();
		handle = bcp11_circuit->insert_gate_name($1, new_gate);
		DEBUG_ASSERT(handle->get_gate() == new_gate);
		$$ = new_gate;
	      }
	  }
	| TRUE			{$$ = bcp11_circuit->new_TRUE(); }
	| FALSE			{$$ = bcp11_circuit->new_FALSE(); }
	| EQUIVf LPAREN formula_list RPAREN
	  {$$ = bcp11_circuit->new_EQUIV($3); delete $3; }
	| IMPLYf LPAREN formula COMMA formula RPAREN
	  {$$ = bcp11_circuit->new_OR(bcp11_circuit->new_NOT($3), $5); }
	| ITEf LPAREN formula COMMA formula COMMA formula RPAREN
	  {$$ = bcp11_circuit->new_ITE($3, $5, $7); }
	| ORf LPAREN formula_list RPAREN
	  {$$ = bcp11_circuit->new_OR($3); delete $3; }
	| ANDf LPAREN formula_list RPAREN
	  {$$ = bcp11_circuit->new_AND($3); delete $3; }
	| EVENf LPAREN formula_list RPAREN
	  {$$ = bcp11_circuit->new_EVEN($3); delete $3; }
	| ODDf LPAREN formula_list RPAREN
	  {$$ = bcp11_circuit->new_ODD($3); delete $3; }
	| NOTf LPAREN formula RPAREN
	  {$$ = bcp11_circuit->new_NOT($3); }
	| treshold_rule
	  {$$ = $1; }
        ;

formula_list:
	  formula	{$$ = new std::list<Gate*>(); $$->push_back($1); }
	| formula_list COMMA formula	{$$ = $1; $$->push_back($3); }
	;

treshold_rule:
	LBRACKET NUM COMMA NUM RBRACKET LPAREN formula_list RPAREN
	  {if($2 > $4) bcp11_error("Threshold min > max\n");
	   $$ = bcp11_circuit->new_CARDINALITY($2,$4,$7); delete $7; }
	;

%%

static void add_variable_definition(char *name)
{
  /*
   * 'gate;' variable definition
   */
  DEBUG_ASSERT(name);
  NameHandle *handle = bcp11_circuit->find_gate(name);
  if(handle)
    {
      Gate *existing_gate = handle->get_gate();
      if(existing_gate->type == Gate::tUNDEF)
	existing_gate->type = Gate::tVAR;
      else if(existing_gate->type == Gate::tVAR)
	;
      else
	bcp11_error("gate '%s' defined twice", name);
      free(name);
      return;
    }
  /* name was not defined */
  Gate *new_gate = bcp11_circuit->new_VAR();
  handle = bcp11_circuit->insert_gate_name(name, new_gate);
  DEBUG_ASSERT(handle->get_gate() == new_gate);
  return;
}


static void add_gate_definition(char *name, Gate *rightsf)
{
  DEBUG_ASSERT(name);
  DEBUG_ASSERT(rightsf);

  NameHandle *handle = bcp11_circuit->find_gate(name);
  if(handle)
    {
      /* name is already declared */
      Gate *existing_gate = handle->get_gate();
  
      if(existing_gate->type != Gate::tUNDEF)
	{
	  bcp11_error("gate '%s' defined twice", name);
	}

      if(existing_gate == rightsf)
	{
	  bcp11_error("'%s := %s;' definition", name, name);
	}

      DEBUG_ASSERT(!existing_gate->children);
      existing_gate->type = Gate::tREF;
      existing_gate->add_child(rightsf);
      free(name);
      return;
    }
  
  /* name is not yet declared */

  if(rightsf->get_first_name() != 0)
    {
      Gate *new_gate = bcp11_circuit->new_REF(rightsf);
      handle = bcp11_circuit->insert_gate_name(name, new_gate);
      DEBUG_ASSERT(handle->get_gate() == new_gate);
      return;
    }

  handle = bcp11_circuit->insert_gate_name(name, rightsf);
  DEBUG_ASSERT(handle->get_gate() == rightsf);
  return;
}

static void start_err(int lineno)
{
  fprintf(stderr,"\n");
  /*if(input_file)fprintf(stderr,"file %s: ",input_file);*/
  if(lineno) fprintf(stderr,"line %d: ", lineno);
}

static void finish_err()
{
  fprintf(stderr, "\n");
  exit(1);
}

void bcp11_error(const char * fmt, ...)
{
  extern char *bcp11_text;
  extern int bcp11_lineno;
  va_list ap;

  start_err(bcp11_lineno);
  va_start(ap, fmt);
  fprintf(stderr,"at token \"%s\": ", bcp11_text);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  finish_err();
}

void bcp11_error2(const char * fmt, ...)
{
  extern int bcp11_lineno;
  va_list ap;

  start_err(bcp11_lineno);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  finish_err();
}

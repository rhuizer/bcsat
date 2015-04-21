/*
 Copyright (C) 2006-2009 Tommi Junttila
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2
 as published by the Free Software Foundation.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "defs.hh"
#include "bc.hh"
#include "timer.hh"

static const char *default_program_name = "bcminisat";

static const char *infilename = 0;
static FILE *infile = stdin;

static bool opt_polarity_cnf = false;
static bool opt_notless = true;
static bool opt_perform_simplifications = true;
static bool opt_print_input_gates = false;
static bool opt_print_solution = true;
static SimplifyOptions simplify_opts;
static bool opt_branch_only_on_input_gates = false;
static bool opt_permute_cnf = false;
static unsigned int opt_permute_cnf_seed = 0;

static void
usage(FILE* const fp, const char* argv0)
{
  const char *program_name;
  
  program_name = rindex(argv0, '/');
  
  if(program_name) program_name++;
  else program_name = argv0;
  
  if(!*program_name) program_name = default_program_name;
  fprintf(fp,
"bcminisat, %s by Tommi Junttila\n"
"Includes MiniSAT (http://minisat.se/) by Niklas Een and Niklas Sorensson.\n"
"\n"
"%s <options> [<circuit file>]\n"
"\n"
"  -input_cuts     only branch on input gates\n"
"  -polarity_cnf   use polarity exploiting CNF translation\n"
"  -nosimplify     do not perform simplifications\n"
"  -nosolution     do not print a satisfying truth assignment\n"
"  -nots           perform an unoptimized CNF-translation with NOT-gates\n"
"  -v              switch verbose mode on\n"
"  -permute_cnf=s  permute CNF variables with seed s\n"
"  -print_inputs   print input gate names\n"
"  <circuit file>  input circuit file (if not specified stdin is used)\n"
	  , BCPACKAGE_VERSION
          , program_name);
}


static void
parse_options(const int argc, const char** argv)
{
  unsigned int seed;

  /* Default simplifcation options */
  simplify_opts.constant_folding = true;
  simplify_opts.downward_bcp = true;
  simplify_opts.remove_duplicate_children = true;
  simplify_opts.remove_g_not_g_children = true;
  simplify_opts.inline_equivalences = true;
  simplify_opts.misc_reductions = true;
  simplify_opts.use_coi = true;
  simplify_opts.absorb_children = SimplifyOptions::CHILDABSORB_NONE;


  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0)
      verbose = true;
    else if(strcmp(argv[i], "-input_cuts") == 0)
      opt_branch_only_on_input_gates = true;
    else if(strcmp(argv[i], "-polarity_cnf") == 0)
      opt_polarity_cnf = true;
    else if(strcmp(argv[i], "-nosimplify") == 0)
      opt_perform_simplifications = false;
    else if(strcmp(argv[i], "-nosolution") == 0)
      opt_print_solution = false;
    else if(strcmp(argv[i], "-nots") == 0)
      opt_notless = false;
    else if(sscanf(argv[i], "-permute_cnf=%u", &seed) == 1)
      {
	opt_permute_cnf = true;
	opt_permute_cnf_seed = seed;
      }
    else if(strcmp(argv[i], "-print_inputs") == 0)
      opt_print_input_gates = true;
    else if(argv[i][0] == '-') {
      fprintf(stderr, "unknown command line argument `%s'\n", argv[i]);
      usage(stderr, argv[0]);
      exit(1);
    }
    else {
      if(infile != stdin) {
	fprintf(stderr, "too many file arguments\n");
	usage(stderr, argv[0]);
	exit(1);
      }
      else {
	infilename = argv[i];
	infile = fopen(argv[i], "r");
	if(!infile) {
	  fprintf(stderr, "cannot open `%s' for input\n", argv[i]);
	  exit(1); }
      }
    }
  }
}



int
main(const int argc, const char** argv)
{
  BC* circuit = 0;
  int result = 0;

  verbstr = stdout;

  parse_options(argc, argv);
  
  Timer timer_total;

  verbose_print("Parsing from %s\n", infilename?infilename:"stdin");
  
  circuit = BC::parse_circuit(infile);
  if(circuit == 0)
    exit(-1);
  if(infilename) fclose(infile);

  verbose_print("Parsing time: %.2lf\n", timer_total.get_duration());
  verbose_print("The circuit has %d gates\n", circuit->count_gates());
  

  if(opt_print_input_gates and verbstr)
    {
      /*
       * Print input gates
       */
      fprintf(verbstr, "The input gates are: ");
      circuit->print_input_gate_names(verbstr, " ");
      fprintf(verbstr, "\n");
    }
  
  /*
   * Mark values of assigned gates
   */
  while(!circuit->assigned_to_true.empty())
    {
      Gate* const gate = circuit->assigned_to_true.front();
      circuit->assigned_to_true.pop_front();
       if(!circuit->force_true(gate))
	goto unsat_exit;
    }
  while(!circuit->assigned_to_false.empty())
    {
      Gate* const gate = circuit->assigned_to_false.front();
      circuit->assigned_to_false.pop_front();
      if(!circuit->force_false(gate))
	goto unsat_exit;
    }

  /*
   * Remove all gate names beginning with the underscore character _
   */
  circuit->remove_underscore_names();


  /*
   * Do the actual solving...
   */
  result = circuit->minisat_solve(opt_perform_simplifications,
				  simplify_opts,
				  opt_polarity_cnf,
				  opt_notless,
				  opt_branch_only_on_input_gates,
				  opt_permute_cnf,
				  opt_permute_cnf_seed
				  );
  
  if(result == 0)
    goto unsat_exit;

  DEBUG_ASSERT(result == 1);
      
  fprintf(stdout, "Satisfiable\n");
  
  /*
   * Print solution
   */
  if(opt_print_solution)
    {
      circuit->print_assignment(stdout);
      fprintf(stdout, "\n");
      fflush(stdout);
    }

  goto clean_and_exit;
  
 unsat_exit:
  fprintf(stdout, "Unsatisfiable\n");

 clean_and_exit:
  verbose_print("Total time: %.2lf\n", timer_total.get_duration());
  
  /* Clean'n'exit */
  delete circuit; circuit = 0;
  return 0;
}

/*
 Copyright (C) 2003-2009 Tommi Junttila
 
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

const char *default_program_name = "bc2cnf";

const char *infilename = 0;
FILE *infile = stdin;

const char *outfilename = 0;
FILE *outfile = stdout;

/* Default options */
static bool opt_cnf_notless = true;
static bool opt_cnf_polarity = false;
static bool opt_cnf_permute = false;
static unsigned int opt_cnf_permute_seed = 0;
static bool opt_perform_coi = true;
static bool opt_perform_simplifications = true;
static bool opt_preserve_all_solutions = false;
static bool opt_print_input_gates = false;

static void
usage(FILE* const fp, const char* argv0)
{
  const char *program_name;
  
  program_name = rindex(argv0, '/');
  
  if(program_name) program_name++;
  else program_name = argv0;
  
  if(!*program_name) program_name = default_program_name;
  fprintf(fp, "bc2cnf, %s\n", BCPACKAGE_VERSION);
  fprintf(fp, "Copyright 2003-2009 Tommi Junttila\n");
  fprintf(fp,
"%s <options> [<circuit file>] [<cnf file>]\n"
"\n"
"  -v              switch verbose mode on\n"
"  -all            preserve all solutions (default: preserve satisfiability)\n"
"  -nosimplify     do not perform simplifications\n"
"  -nocoi          do not perform final cone of influence\n"
"  -nots           perform an unoptimized CNF-translation with NOT-gates\n"
"  -polarity_cnf   use polarity exploiting CNF translation\n"
"  -permute_cnf=s  permute CNF variables with seed s\n"
"  -print_inputs   print input gate names\n"
"  <circuit file>  input circuit file (if not specified, stdin is used)\n"
"  <cnf file>      output cnf file (if not specified, stdout is used)\n"
          ,program_name);
}


static void
parse_options(const int argc, const char** argv)
{
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0)
      verbose = true;
    else if(strcmp(argv[i], "-polarity_cnf") == 0)
      opt_cnf_polarity = true;
    else if(sscanf(argv[i], "-permute_cnf=%u", &opt_cnf_permute_seed) == 1)
      opt_cnf_permute = true;
    else if(strcmp(argv[i], "-all") == 0)
      opt_preserve_all_solutions = true;
    else if(strcmp(argv[i], "-nosimplify") == 0)
      opt_perform_simplifications = false;
    else if(strcmp(argv[i], "-nocoi") == 0)
      opt_perform_coi = false;
    else if(strcmp(argv[i], "-nots") == 0)
      opt_cnf_notless = false;
    else if(strcmp(argv[i], "-print_inputs") == 0)
      opt_print_input_gates = true;
    else if(argv[i][0] == '-') {
      fprintf(stderr, "unknown command line argument `%s'\n", argv[i]);
      usage(stderr, argv[0]);
      exit(1);
    }
    else {
      if(infile != stdin) {
	if(outfile != stdout) {
	  fprintf(stderr, "too many file arguments\n");
	  usage(stderr, argv[0]);
	  exit(1);
	}
	outfilename = argv[i];
	outfile = fopen(argv[i], "w");
	if(!outfile) {
	  fprintf(stderr, "cannot open `%s' for output\n", argv[i]);
	  exit(1); }
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
  BC *circuit = 0;
  int max_var_num;

  verbstr = stdout;

  parse_options(argc, argv);
  
  if(verbose)
    {
      fprintf(verbstr, "parsing from %s\n", infilename?infilename:"stdin");
      fflush(verbstr);
    }
  
  circuit = BC::parse_circuit(infile);
  if(circuit == 0)
    exit(1);
    
  if(infilename)
    fclose(infile);

  if(verbose)
    {
      fprintf(verbstr, "The circuit has %d gates\n", circuit->count_gates());
      fflush(verbstr);
    }

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
      Gate *gate = circuit->assigned_to_true.front();
      circuit->assigned_to_true.pop_front();
      if(!circuit->force_true(gate))
	goto unsat_exit;
    }
  while(!circuit->assigned_to_false.empty())
    {
      Gate *gate = circuit->assigned_to_false.front();
      circuit->assigned_to_false.pop_front();
      if(!circuit->force_false(gate))
	goto unsat_exit;
    }

  /*
   * Remove all gate names beginning with the underscore character _
   */
  circuit->remove_underscore_names();

  /*
   * Set flags for simplifications
   */
  circuit->preserve_all_solutions = opt_preserve_all_solutions;

  /*
   * Simplify or at least share structure
   */
  if(opt_perform_simplifications)
    {
      if(!circuit->simplify(false))
	goto unsat_exit;
    }
  else
    {
      if(!circuit->share())
	goto unsat_exit;
    }




  if(!circuit->cnf_normalize())
    goto unsat_exit;
  
  if(opt_perform_simplifications)
    {
      if(!circuit->simplify(true))
	goto unsat_exit;
    } 
  else
    {
      if(!circuit->share())
	goto unsat_exit;
    }
  
  
  /*
   * Print some statistics
   */
  if(verbose)
    {
      unsigned int max_min_height, max_max_height;
      circuit->compute_stats(max_min_height, max_max_height);
      fprintf(verbstr, "The max-min height of the circuit is %u\n",
              max_min_height);
      fprintf(verbstr, "The max-max height of the circuit is %u\n",
              max_max_height);
      fflush(verbstr);
    }

  /*
   * Find the relevant gates and number them in temp field (aka COI)
   */
  {
    int nof_relevant_gates = 0;
    circuit->reset_temp_fields(-1);
    for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
      {
	if(opt_perform_coi == false or
	   (gate->determined and !gate->is_justified()))
	  gate->mark_coi(nof_relevant_gates);
      }
    if(verbose)
      {
	fprintf(verbstr, "The circuit has %d relevant gates\n",
		nof_relevant_gates);
	fflush(verbstr);
      }
    if(nof_relevant_gates == 0)
      {
	goto sat_exit;
      }
  }
  

  /*
   * Renumber the gates in temp-fields and
   * compute the number of relevant input gates
   */
  {
    int gate_num = 0;
    unsigned int nof_relevant_input_gates = 0;
    for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
      {
        if(gate->temp == -1) {
          /* Not relevant */
          continue;
        }
        if(opt_cnf_notless and gate->type == Gate::tNOT) {
          /* NOT-less translation */
          assert(!gate->determined);
          assert(gate->children->child->type != Gate::tNOT);
          gate->temp = -1;
        }
        else {
          gate->temp = ++gate_num;
        }
	if(gate->type == Gate::tVAR)
          nof_relevant_input_gates++;
      }
    max_var_num = gate_num;
    assert(max_var_num > 0);
    
    if(verbose) {
      fprintf(verbstr, "The circuit has %d relevant input gates\n",
              nof_relevant_input_gates);
      fflush(verbstr);
    }
  }


  /*
   * Permute the CNF variable indices if required
   */
  if(opt_cnf_permute)
    {
      if(verbose)
	{
	  fprintf(verbstr, "Permuting the CNF variables..."); fflush(verbstr);
	}
      unsigned int* const perm = my_perm(max_var_num, opt_cnf_permute_seed);
      for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
	{
	  assert(gate->temp <= max_var_num);
	  if(gate->temp <= 0)
	    continue;
	  gate->temp = perm[gate->temp];
	  assert(gate->temp > 0 && gate->temp <= max_var_num);
	}
      free(perm);
      if(verbose)
	{
	  fprintf(verbstr, "done\n"); fflush(verbstr);
	}
    }

  /*
   * Print info header
   */
  fprintf(outfile, "\
c This is a CNF SAT formula in the DIMACS CNF format,\n\
c produced with the bc2cnf translator by Tommi Junttila;\n\
c see http://www.tcs.hut.fi/~tjunttil/circuits/index.html\n\
");
 
  /*
   * Print translation table
   */
  for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
    {
      assert(gate->temp <= max_var_num);
      if(gate->temp <= 0)
	continue;
      if(opt_cnf_polarity && gate->type != Gate::tVAR)
	continue;
      const Handle* handle = gate->handles;
      while(handle) {
	if(handle->get_type() == Handle::ht_NAME) {
	  const char* const name = ((NameHandle *)handle)->get_name();
	  DEBUG_ASSERT(name);
	  if(opt_cnf_notless && gate->type == Gate::tNOT) {
	    fprintf(outfile, "c %s <-> %d\n", name,
		    -gate->children->child->temp);
	  } else {
	    fprintf(outfile, "c %s <-> %d\n", name, gate->temp);
	  }
	}
	handle = handle->get_next();
      }
    }


  /*
   * Compute polarity info if needed
   */
  if(opt_cnf_polarity)
    circuit->mir_compute_polarity_information();


  {
    /*
     * Scan the CNF in order to find the number of clauses,
     * required by the DIMACS header
     */
    if(verbose)
      {
	fprintf(verbstr, "Computing cnf size...");
	fflush(verbstr);
      }
    unsigned int nof_cnf_clauses = 0;
    std::list<std::vector<int> *> clauses;
    for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
      {
	DEBUG_ASSERT(gate->temp == -1 or
		     (0 < gate->temp and gate->temp <= max_var_num));
        if(gate->temp == -1) {
          /* Not relevant */
          continue;
        }
        /*
         * Translation clauses
         */
	if(opt_cnf_polarity)
	  gate->cnf_get_clauses_polarity(clauses, opt_cnf_notless);
	else
	  gate->cnf_get_clauses(clauses, opt_cnf_notless);

	nof_cnf_clauses += clauses.size();
	while(!clauses.empty())
	  {
	    delete clauses.back();
	    clauses.pop_back();
	  }

	/*
         * Unit clauses for constrained gates
         */
        if(gate->determined)
	  nof_cnf_clauses += 1;
	else
	  {
	    /* The following cases should really not happen... */
	    if(gate->type == Gate::tTRUE)
	      nof_cnf_clauses += 1;
	    else if(gate->type == Gate::tFALSE)
	      nof_cnf_clauses += 1;
	  }
      }

    if(verbose)
      {
	fprintf(verbstr, " done\n");
	fprintf(verbstr, "The cnf has %d variables and %d clauses\n",
		max_var_num, nof_cnf_clauses);
	fprintf(verbstr, "Printing cnf...");
	fflush(verbstr);
      }

    /*
     * Print DIMACS header
     */
    fprintf(outfile, "p cnf %d %u\n", max_var_num, nof_cnf_clauses);

    /*
     * Actually print the clauses
     */
    unsigned int nof_cnf_clauses_printed = 0;
    for(Gate *gate = circuit->first_gate; gate; gate = gate->next)
      {
        if(gate->temp == -1) {
          /* Not relevant */
          continue;
        }
	assert(gate->temp > 0 && gate->temp <= max_var_num);
        /*
         * Get clauses
         */
	if(opt_cnf_polarity)
	  gate->cnf_get_clauses_polarity(clauses, opt_cnf_notless);
	else
	  gate->cnf_get_clauses(clauses, opt_cnf_notless);

        while(!clauses.empty())
	  {
	    std::vector<int> *cl = clauses.back();
	    clauses.pop_back();
	    for(std::vector<int>::iterator li = cl->begin();
		li != cl->end();
		li++)
	      {
		const int lit = *li;
		assert(lit != 0 && abs(lit) <= max_var_num);
		fprintf(outfile, "%d ", lit);
	      }
	    fprintf(outfile, "0\n");
	    nof_cnf_clauses_printed++;
	    delete cl;
	  }
	/*
         * Add unit clauses for constrained gates
         */
        if(gate->determined)
	  {
	    fprintf(outfile, "%s%d 0\n", gate->value?"":"-", gate->temp);
	    nof_cnf_clauses_printed++;
	  }
	else
	  {
	    /* The following cases should really not happen... */
	    if(gate->type == Gate::tTRUE)
	      {
		fprintf(outfile, "%d 0\n", gate->temp);
		nof_cnf_clauses_printed++;
	      }
	    else if(gate->type == Gate::tFALSE)
	      {
		fprintf(outfile, "-%d 0\n", gate->temp);
		nof_cnf_clauses_printed++;
	      }
	  }
      }
    assert(nof_cnf_clauses_printed == nof_cnf_clauses);

    if(verbose)
      {
	fprintf(verbstr, " done\n");
	fflush(verbstr);
      }
  }
  
  /* Clean'n'exit */
  delete circuit; circuit = 0;
  return 0;

 sat_exit:
  if(verbose)
    {
      fprintf(verbstr, "The circuit was found satisfiable, constructing a truth assignment... ");
      fflush(verbstr);
    }

  /*
   * Assign irrelevant input gates to arbitrary values
   */
  for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
    {
      if(gate->type == Gate::tVAR && !gate->determined)
	{
	  assert(gate->temp == -1);
	  gate->determined = true;
	  gate->value = false;
	}
    }

  /*
   * Evaluate rest of the irrelevant gates
   */
  for(Gate *gate = circuit->first_gate; gate; gate = gate->next)
    {
      if(!gate->determined)
	{
	  const bool evaluation_ok = gate->evaluate();
	  if(!evaluation_ok)
	    internal_error("%s:%u: Evaluation error",__FILE__,__LINE__);
	  DEBUG_ASSERT(gate->determined);
	}
    }

  /*
   * Check consistency
   */
  if(!circuit->check_consistency())
    {
      internal_error("%s:%u: Consistency check failed",__FILE__,__LINE__);
      exit(-1);
    }

  if(verbose)
    {
      fprintf(verbstr, "done\n");
      fprintf(verbstr, "Printing a dummy cnf containing the solution in comments... ");
      fflush(verbstr);
    }

  /*
   * Print satisfying truth assignment
   */
  fprintf(outfile, "c The instance was satisfiable\n");
  for(Gate *gate = circuit->first_gate; gate; gate = gate->next)
    {
      assert(gate->determined);
      Handle *handle = gate->handles;
      while(handle) {
	if(handle->get_type() == Handle::ht_NAME) {
	  const char *name = ((NameHandle *)handle)->get_name();
	  DEBUG_ASSERT(name);
	  fprintf(outfile, "c %s <-> %s\n", name, gate->value?"T":"F");
	}
	handle = handle->get_next();
      }
    }
  /* And a dummy satisfiable CNF */
  fprintf(outfile, "p cnf 1 1\n");
  fprintf(outfile, "1 0\n");
  if(verbose)
    {
      fprintf(verbstr, "done\n");
      fflush(verbstr);
    }

  /* Clean'n'exit */
  delete circuit; circuit = 0;
  return 0;


 unsat_exit:
  if(verbose)
    {
      fprintf(verbstr, "The circuit was found unsatisfiable, printing a dummy unsatisfiable cnf\n");
      fflush(verbstr);
    }
  /*
   * Print a small unsatisfiable CNF
   */
  fprintf(outfile, "c The instance was unsatisfiable\n");
  fprintf(outfile, "p cnf 1 2\n");
  fprintf(outfile, "1 0\n");
  fprintf(outfile, "-1 0\n");

  /* Clean'n'exit */
  delete circuit; circuit = 0;
  return 0;
}

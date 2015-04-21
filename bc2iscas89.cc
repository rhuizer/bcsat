/*
 Copyright (C) 2008-2009 Tommi Junttila
 
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

const char *default_program_name = "bc2iscas89";

const char *infilename = 0;
FILE *infile = stdin;

const char *outfilename = 0;
FILE *outfile = stdout;

/* Default options */
static bool opt_perform_coi = true;
static bool opt_perform_simplifications = true;
static bool opt_preserve_all_solutions = false;
static bool opt_print_input_gates = false;

static void
usage(FILE* const fp, const char* argv0)
{
  const char* program_name;
  
  program_name = rindex(argv0, '/');
  
  if(program_name) program_name++;
  else program_name = argv0;
  
  if(!*program_name) program_name = default_program_name;
  fprintf(fp, "bc2iscas89, %s\n", BCPACKAGE_VERSION);
  fprintf(fp, "Copyright Tommi Junttila\n");
  fprintf(fp,
"%s <options> [<circuit file>] [<cnf file>]\n"
"\n"
"  -v              switch verbose mode on\n"
"  -all            preserve all solutions (default: preserve satisfiability)\n"
"  -nosimplify     do not perform simplifications\n"
"  -nocoi          do not perform final cone of influence reduction\n"
"  -print_inputs   print input gate names\n"
"  <circuit file>  input circuit file (if not specified, stdin is used)\n"
"  <iscas file>    output iscas89 file (if not specified, stdout is used)\n"
          ,program_name);
}


static void
parse_options(const int argc, const char** argv)
{
  for(int i = 1; i < argc; i++)
    {
      if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-verbose") == 0)
	verbose = true;
      else if(strcmp(argv[i], "-all") == 0)
	opt_preserve_all_solutions = true;
      else if(strcmp(argv[i], "-nosimplify") == 0)
	opt_perform_simplifications = false;
      else if(strcmp(argv[i], "-nocoi") == 0)
	opt_perform_coi = false;
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



int main(int argc, const char **argv)
{
  BC *circuit = 0;
  int max_var_num;
  std::vector<Gate *> *ordering = 0;

  const char* const iscas_prefix = "\
# Generated with the bc2iscas89 tool, version 0.40\n\
# See http://users.ics.aalto.fi/tjunttil/circuits/index.html\n\
# for the BCpackage Boolean circuit tool set\n\
";


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

  if(opt_print_input_gates && verbstr)
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
      SimplifyOptions opts;
      if(!circuit->simplify(opts))
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
      SimplifyOptions opts;
      opts.preserve_cnf_normalized_form = true;
      if(!circuit->simplify(opts))
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
	if(opt_perform_coi == false ||
	   (gate->determined && !gate->is_justified()))
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
	gate->temp = ++gate_num;
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


  if(verbose)
    {
      fprintf(verbstr, "Printing the circuit in the iscas89 format...");
      fflush(verbstr);
    }


  /*
   * print iscas89 file prefix
   */
  fprintf(outfile, "%s", iscas_prefix);

  ordering = circuit->get_top_down_ordering();
  /*
   * Print translation table
   */
  for(std::vector<Gate* >::const_reverse_iterator gi = ordering->rbegin();
     gi != ordering->rend();
     gi++)
    {
      Gate * const gate = *gi;
      assert(gate->temp <= max_var_num);
      if(gate->temp <= 0)
	continue;
      gate->write_iscas89_map(outfile);
    }


  /*
   * Declare input gates
   */
  for(std::vector<Gate* >::const_reverse_iterator gi = ordering->rbegin();
     gi != ordering->rend();
     gi++)
    {
      Gate * const gate = *gi;
      assert(gate->temp <= max_var_num);
      if(gate->temp <= 0)
	continue;
      if(gate->children)
	continue;
      fprintf(outfile, "INPUT(");
      gate->write_iscas89_name(outfile);
      fprintf(outfile, ")\n");
    }

  /*
   * Declare output gate
   */
  fprintf(outfile, "OUTPUT(out)\n");


  /*
   * Actually convert the gates
   */
  for(std::vector<Gate* >::const_reverse_iterator gi = ordering->rbegin();
     gi != ordering->rend();
     gi++)
    {
      Gate * const gate = *gi;
      if(gate->temp == -1) {
	/* Not relevant */
	continue;
      }
      assert(gate->temp > 0 && gate->temp <= max_var_num);
      gate->write_iscas89(outfile);
    }

  /*
   * Build output gate
   */
  {
    for(std::vector<Gate* >::const_reverse_iterator gi = ordering->rbegin();
	gi != ordering->rend();
	gi++)
      {
	Gate * const gate = *gi;
	if(gate->temp == -1) {
	  /* Not relevant */
	  continue;
	}
	if(!gate->determined || gate->value == true)
	  continue;
	fprintf(outfile, "n_%u = NOT(", gate->index);
	gate->write_iscas89_name(outfile);
	fprintf(outfile, ")\n");
      }
    const char *sep = "";
    fprintf(outfile, "out = AND(");
    for(std::vector<Gate* >::const_reverse_iterator gi = ordering->rbegin();
	gi != ordering->rend();
	gi++)
      {
	Gate * const gate = *gi;
	if(gate->temp == -1) {
	  /* Not relevant */
	  continue;
	}
	if(!gate->determined)
	  continue;
	fprintf(outfile, "%s", sep);
	sep = ",";
	if(gate->value)
	  gate->write_iscas89_name(outfile);
	else
	  fprintf(outfile, "n_%u", gate->index);
      }
    fprintf(outfile, ")\n");
  }
  fprintf(outfile, "#@ out\n");

  if(verbose)
    {
      fprintf(verbstr, " done\n");
      fflush(verbstr);
    }

  if(ordering) {delete ordering; ordering = 0; }
  
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
  for(Gate* gate = circuit->first_gate; gate; gate = gate->next)
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
      fprintf(verbstr, "Printing a dummy iscas89 file containing the solution in comments... ");
      fflush(verbstr);
    }

  /*
   * Print satisfying truth assignment
   */
  fprintf(outfile, "%s", iscas_prefix);
  fprintf(outfile, "# The instance was satisfiable\n");
  for(Gate *gate = circuit->first_gate; gate; gate = gate->next)
    {
      assert(gate->determined);
      Handle *handle = gate->handles;
      while(handle) {
	if(handle->get_type() == Handle::ht_NAME) {
	  const char *name = ((NameHandle*)handle)->get_name();
	  DEBUG_ASSERT(name);
	  fprintf(outfile, "# %s <- %s\n", name, gate->value?"T":"F");
	}
	handle = handle->get_next();
      }
    }
  /* And a dummy satisfiable CNF */
  fprintf(outfile, "INPUT(g_1)\n");
  fprintf(outfile, "OUTPUT(g_2)\n");
  fprintf(outfile, "g_2 = NOT(g_1)\n");
  fprintf(outfile, "#@ g_2\n");
  if(verbose)
    {
      fprintf(verbstr, "done\n");
      fflush(verbstr);
    }

  /* Clean'n'exit */
  if(ordering) {delete ordering; ordering = 0; }
  delete circuit; circuit = 0;
  return 0;


 unsat_exit:
  if(verbose)
    {
      fprintf(verbstr, "The circuit was found unsatisfiable, printing a dummy unsatisfiable iscas89 file\n");
      fflush(verbstr);
    }
  /*
   * Print a small unsatisfiable ISCAS89 file
   */
  fprintf(outfile, "%s", iscas_prefix);
  fprintf(outfile, "# The instance was unsatisfiable\n");
  fprintf(outfile, "INPUT(g_1)\n");
  fprintf(outfile, "OUTPUT(g_3)\n");
  fprintf(outfile, "g_2 = NOT(g_1)\n");
  fprintf(outfile, "g_3 = AND(g_1,g_2)\n");  
  fprintf(outfile, "#@ g_3\n");
  
  /* Clean'n'exit */
  if(ordering) {delete ordering; ordering = 0; }
  delete circuit; circuit = 0;
  return 0;
}

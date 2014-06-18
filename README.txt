Boolean circuit tool package (BCpackage) version 0.35

(c) 2002-2009 Tommi Junttila

Released under the the GNU General Public License version 2,
see the attached file LICENSE.txt for the full license text.

The home page of the BC tool package is
              http://www.tcs.hut.fi/~tjunttil/circuits/
The Boolean circuit file format accepted by the tools in the package
is also explained on that page.


The BC tool package consists of the following utilities:

- bc2cnf
  A tool that translates Boolean circuits into CNF formula in the DIMCAS
  format accepted by the most of the state-of-the-art SAT solvers.
  Compile with 'make bc2cnf' and say ./bc2cnf -help' to get usage information.

- bc2edimacs and edimacs2bc
  Tools that convert Boolean circuits into the extended (non-clausal) DIMACS
  format and vice versa.

- bc2iscas89
  A tool for onverting Boolean circuits to ISCAS89 format.

- bczchaff
  A Boolean circuit front-end to the ZChaff solver available at
  http://www.princeton.edu/~chaff/zchaff.html
  Converts the argument Boolean circuit internally into a CNF formula,
  calls ZChaff, and represents the result in the terms of the circuit.
  The ZChaff solver is not included in the BC package but you have to download
  it from the above web page, unarchive and compile it, modify the Makefile
  of BCpackage to include the ZChaff source directory, and compile bczchaff
  with 'make bczchaff'.
  Should work at least with ZChaff versions zchaff.2007.3.12 and
  zchaff.64bit.2007.3.12 (for 64bit machines).

- bcminisat2core and bcminisat2simp
  The same as bczchaff but with MiniSat version 2 available at
  http://minisat.se/
  bcminisat2core includes the plain MiniSat2 without the preprocessor
  simplifier, while bcminisat2simp includes the preprocessor, too.
  Should work at least with minisat2 version 070721.

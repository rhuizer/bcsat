Boolean circuit tool package (BCpackage) version 0.40

(c) 2002-2015 Tommi Junttila

Released under the the GNU General Public License version 2,
see the attached file LICENSE.txt for the full license text.

The home page of the BC tool package is
        http://users.ics.aalto.fi/tjunttil/circuits/index.html
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
  The ZChaff solver is not included in the BC package but you have to
  - download it from the above web page
  - unzip it to some directory
  - set the ZCHAFF_PATH variable in the Makefile of BCpackage to point
    to that directory, and
  - say 'make bczchaff'.
  Should work at least with ZChaff versions zchaff.2007.3.12 and
  zchaff.64bit.2007.3.12 (for 64bit machines).

- bcminisat220core and bcminisat220simp
  The same as bczchaff but with MiniSat version 2.2.0 available at
  http://minisat.se/MiniSat.html
  bcminisat220core includes the plain MiniSat 2.2.0 without the preprocessor
  simplifier, while bcminisat220simp includes the preprocessor, too.

  Build with 'make bcminisat220core' and 'make bcminisat220simp'
  after you have downloaded and unarchived MiniSat as well as
  set the MINISAT220_PATH variable in the Makefile appropriately.

- bcminisat2core and bcminisat2simp
  The same as  bcminisat220core and bcminisat220simp above but
  with an older vertion MiniSat2-070721 available at
  http://minisat.se/MiniSat.html

  Build with 'make bcminisat2core' and 'make bcminisat2simp'
  after you have downloaded and unarchived MiniSat as well as
  set the MINISAT2_PATH variable in the Makefile appropriately.

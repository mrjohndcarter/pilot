# About Pilot

Experience from SHARCNET shows that beginning scientific programmers face difficulties becoming proficient in using MPI. Its API is large and daunting, and presents multiprogramming hazards that even trained programmers find difficult to cope with. When user errors lead to deadlock, since the program appears to still be running, detecting the condition, let alone diagnosing its cause, can be troublesome.

The authors have created the Pilot library (in C) as a layer on top of standard MPI. In terms of number of functions, it is less than one-tenth the size of MPI-2. Instead of dealing with ranks, communicators, tags, buffers and other low-level issues, programmers employ two high-level components—processes and channels—to architect their cluster applications. These abstractions are taken from the classical process algebra Communicating Sequential Processes (CSP), but Pilot users need not concern themselves with formal methods. It is the theoretical characteristics of these components that make it harder to inadvertently program communication errors.

* Processes have their usual MPI meaning (i.e., Pilot is still a SPMD approach), but the code for each process is broken out into separate functions (similar to POSIX threads), which can make for better software quality factors than interleaving code.
* Channels are the only means of interprocess communication. They have four properties: (1) point-to-point, being bound to a specific pair of processes; (2) one-way, such that one process writes to the channel and the other process reads; (3) synchronous, so that execution of the reader and writer only proceed after the message is passed; (4) not typed, so that a given channel may be used to communicate any type of data.

# People
## Faculty
* Dr. William Gardner
* Dr. Gary Grewal

## Graduate Students
* Sid Bao
* John Carter

# Installation

Here's what you need to get Pilot compiled and installed:
* The Pilot source distribution of your choice
* Your favourite MPI implementation
* If you want to be able to generate/view visual logs of Pilot API calls, you will need the MPE library, which includes the Jumpshot-4 viewer (requires JDK 8 or higher). MPE can be downloaded for free from Argonne National Lab.
* If you would like to run the unit tests, you will need CUnit (version 2.1 or higher)
* To generate and install the documentation, you will need Python (2.4 or higher) with Docutils, and Doxygen (1.5.6 or higher)

## Building Pilot

### Configure the Makefile

*NOTE: You can avoid having to edit the Makefile if you simply want to define the PREFIX and MPEHOME variables. In that case you can type "make MPEHOME=/that/path" and "make install PREFIX=/your/path" at the Compile step below.*

Change the variable PREFIX to point to the directory that you wish pilot to be installed in. The pilot header files will be installed in $(PREFIX)/include and the library will be installed in $(PREFIX)/lib. Note that if these directories do not already exist, an attempt is made to create them.

If you installed MPE, uncomment and change the variable MPEHOME to its location. This should be the parent directory of "include" and "lib". If you leave the variable commented out, Pilot will not attempt to build with the MPE library and selecting "-pisvc=j" on the command line will give a warning.

If you plan on running the tests, you may need to edit tests/Makefile. Uncomment and change CUNITHOME to where you installed that package. If you are building Pilot with MPE, you also need to uncomment/change MPEHOME and LDFLAGS.

If your C compiler for MPI is not installed in a standard place, you will also have to change the variable CC to point to the proper compiler.
Note that if you install Pilot to a non-standard directory, then you will have to make sure that the install directory is in your include and library path. You can use the mpicc -I and -L options to set those paths.

### Compile

Once the Makefile has been configured, you can run:

```
$ make
$ make install
```

*If you don't need the Fortran API, type "make cpilot" instead of "make".*

For information on how to build and run the unit tests, see the README in the "tests" directory.

## Installing the Manual Pages

Pilot comes with a set of man pages to serve as reference material. These man pages are in the "docs" directory. If you would like to install these pages for system wide access, you can set the DOC_PREFIX variable in the main Makefile and then run:

```
$ make install-man
```

If you would like to view these man pages without installing them, you can change your MANPATH to include the "docs" directory. Alternatively, you can run man in the current directory and point it to the docs like so:
$ man -M docs/man <page>
Where <page> is the man page you want to view.


## Contact Us
If you have any question about Pilot or find issues while using it, please open a Github issue.

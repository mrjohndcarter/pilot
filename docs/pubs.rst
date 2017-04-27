==================
Pilot Publications
==================
:Author: William Gardner
:Updated: 23 January 2015

Tutorials Conducted
-------------------
Pilot training has been featured at major international parallel computing
conferences, and at Canadian regional events.

==== ===============
2012 PACT, Minneapolis, MN
2011 PPoPP, Austin, TX; SHARCNET Summer School, Oakville, ON
2010 PPoPP, Bangalore, India; HPCS, Toronto, ON; SHARCNET Summer School, Oakville, ON
2009 HPCS, Kingston, ON; TIC-STH, Toronto, ON
==== ===============

Pilot is regularly taught in U. of Guelph's undergraduate course in `Parallel Programming`__.

__ http://www.uoguelph.ca/~gardnerw/courses/cis3090/index.htm

On Pilot
--------
2017
  Sid Bao, "Visualization Tool for Debugging Pilot Cluster Programs," MSc thesis, University of Guelph, School of Computer Science. `[Guelph Atrium]`__

__ http://hdl.handle.net/10214/10193

2014
  W.B. Gardner, J.D. Carter, "Using the Pilot Library to Teach Message-Passing Programming," Workshop on Education for High-Performance Computing (EduHPC) at *Intl. Conf. for High Performance Computing, Networking, Storage, and Analysis (SC-14),* New Orleans, Nov. 16-21, pp. 1-8. `[ACM Digital Library]`__

__ http://dl.acm.org/citation.cfm?id=2690855

2012
  Clay Breshears & Kathy Farrel, co-hosts on Intel Software Network TV, interviewed Dr. Gardner on Parallel Programming Talk about the Pilot library, Nov. 9. `[Watch the show]`__

__ http://software.intel.com/en-us/blogs/2011/11/09/pilot-c-library-with-university-of-guelphs-dr-william-gardner-ppt-126/

2011
  Dan Recoskie, W. Gardner, P. Matsakis, "Ease of Combining Multicore and Message-Passing on Clusters with OpenMP and Pilot," SHARCNET Research Day, Oakville, May 19. *Dan demonstrates how to use Pilot and OpenMP in the same program, to exploit two levels of concurrency, including the commands for submitting such jobs to SHARCNET.* `[PDF]`__

__ http://www.uoguelph.ca/%7Egardnerw/pubs/SNResDay11.pdf

2010
  John Carter, W.B. Gardner, G. Grewal, "The Pilot Approach to Cluster Programming in C," Workshop on Parallel and Distributed Scientific and Engineering Computing (PDSEC-10), *Proc. of the 24th IEEE International Parallel & Distributed Processing Symposium, Workshops and Phd Forum,* Atlanta, Apr. 19-23, pp. 1-8. `[IEEE Computer Society Digital Library]`__

__ http://www.computer.org/csdl/proceedings/ipdpsw/2010/6533/00/05470772-abs.html

  John Carter, W.B. Gardner, G. Grewal, "The Pilot library for novice MPI programmers," *15th ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP 2010),* poster paper, Bangalore, India, Jan. 9-14, pp. 351-352.

2009
  Natalie Girard, G. Grewal, W. Gardner, "Comparison of Pilot and MPI implementations of Parallel Scatter Search," SHARCNET Research Day, poster, Waterloo, May 21. *Best poster award.*

2006
  John Carter, W.B. Gardner, "A Formal CSP Framework for Message-passing HPC Programming," *IEEE Canada 19th Annual Canadian Conference on Electrical and Computer Engineering (CCECE 2006)*, Ottawa, May 7-10, pp. 944-948. *Carter's original idea called CSP4MPI, the precursor to Pilot.* `[PDF]`__

__ http://www.uoguelph.ca/%7Egardnerw/pubs/CCECE06.pdf

On AutoPilot
------------
AutoPilot was created to ease the difficulties in programming an advanced embedded multicore chip, the IMAPCAR2 from Renesas Electronics, used chiefly in automotive image-processing applications.
The chip can be dynamically reconfigured for 128-way SIMD processing, for MIMD processing as a cluster of control processor (CP) plus 32 processing units (PUs), or half and half.
The big challenges for programming MIMD mode are (1) the lack of cache coherency making conventional pthreads coding treacherous since shared variables are not automatically synchronized, and (2) the danger of creating a deadlock by inadvertantly flooding the C-ring used for inter-PU message passing.
Pilot's process/channel architecture and distinctive API were layered onto the native SDK to make message-passing programming more accessible to IMAPCAR2 programmers, thus getting away from the error-prone use of pthreads, and also safer, since C-ring flood avoidance and many other low-level complications are automatically handled by AutoPilot.
This technique is more generally applicable for promoting message-passing parallel programming on complex multicore architectures which lack cache coherency.

2013
  Ben Kelly, William B. Gardner, Shorin Kyo, "AutoPilot: message passing parallel programming for a cache incoherent embedded manycore processor," ACM International Workshop on Manycore Embedded Systems (MES-13), *Proc. International Symposium on Computer Architecture (ISCA-13),* Tel Aviv, Jun. 23-27, pp. 62-65. `[PDF]`__

__ http://www.uoguelph.ca/%7Egardnerw/pubs/MES-13.pdf

  Benjamin Kelly, "AutoPilot: A Message-Passing Parallel Programming Library for the IMAPCAR2," MSc thesis, University of Guelph, School of Computer Science. `[Guelph Atrium]`__ `[source code]`__

__ http://hdl.handle.net/10214/5924
__ http://www.uoguelph.ca/%7Egardnerw/pubs/autopilot.zip

On CellPilot
------------
CellPilot was created to ease the difficulties in programming a hybrid HPC cluster consisting of Cell BEs and other nodes.  Pilot's process/channel architecture was extended to the Cell's SPUs, and used to communicate between any pair of processes, whether located on PPEs, SPUs, or non-Cell nodes.  CellPilot handles the launching of SPU processes, shielding the programmer from numerous complex low-level Cell library calls.

2012
  Natalie Girard, "CellPilot: An Extension of the Pilot Library for Cell Broadband Engine Processors and Heterogeneous Clusters," MSc thesis, University of Guelph, School of Computer Science. `[Guelph Atrium]`__

__ http://hdl.handle.net/10214/3279

2011
  Natalie Girard, J. Carter, W. Gardner, G. Grewal, "A seamless communication solution for hybrid cell clusters," Fourth International Workshop on Parallel Programming Models and Systems Software for High-End Computing (P2S2), *Proc. International Conference on Parallel Processing (ICPP-2012),* Taibei, Sep. 13-16, pp. 259-268.

2010
  Natalie Girard, J. Carter, W. Gardner, G. Grewal, "The CellPilot library: Seamless end-to-end communication for heterogeneous clusters," High Performance Computing Symposium (HPCS 2010), expanded poster paper, *Journal of Physics: Conference Series,* Vol. 256, No. 1, Toronto, June 5-9, p.8-13. *Sandia National Labs "Sandia Awesome Award."*

  Natalie Girard, J. Carter, W. Gardner, G. Grewal, "The CellPilot library: Seamless end-to-end communication for heterogeneous clusters," SHARCNET Research Day, poster, York, May 6. *"Best use of HPC" poster award.*

## Quick Start

#### Dependencies Installation

```

sudo apt-get install gcc-5 g++-5 python python3 python3-dev

sudo apt-get install python3-setuptools git mercurial

sudo apt-get install qt5-default mercurial

sudo apt-get install gir1.2-goocanvas-2.0 python-gi python-gi-cairo python-pygraphviz python3-gi python3-gi-cairo python3-pygraphviz gir1.2-gtk-3.0 ipython ipython3  

sudo apt-get install openmpi-bin openmpi-common openmpi-doc libopenmpi-dev

sudo apt-get install autoconf cvs bzr unrar

sudo apt-get install gdb valgrind 

sudo apt-get install uncrustify

sudo apt-get install doxygen graphviz imagemagick

sudo apt-get install texlive texlive-extra-utils texlive-latex-extra texlive-font-utils dvipng latexmk

sudo apt-get install python3-sphinx dia 

sudo apt-get install gsl-bin libgsl-dev libgsl23 libgslcblas0

sudo apt-get install tcpdump

sudo apt-get install sqlite sqlite3 libsqlite3-dev

sudo apt-get install libxml2 libxml2-dev

sudo apt-get install cmake libc6-dev libc6-dev-i386 libclang-6.0-dev llvm-6.0-dev automake 

sudo apt-get install libgtk2.0-0 libgtk2.0-dev

sudo apt-get install vtun lxc uml-utilities

sudo apt-get install libboost-signals-dev libboost-filesystem-dev

sudo apt-get install fftw3 fftw3-dev
```

#### Build

CC='gcc-5' CXX='g++-5' ./waf configure

./waf

#### Simple run of ICC

./waf --run "periodcCCEvaDumbbell"

Outputs including RTT, RTT_standing (Stand), Queuing delay (Qd), Time stamp (now), cwnd, Target rate (Trate) and so on, which are represented as bellow, indicates that the simulation is run successfully.

`0x[...] rttI [...] Stand [...] Old [...] RTTmin [...] Qd [...] now [...] ......`

# rootbr
This is a terminal program that can be used to quickly and easily explore the contents of a CERN ROOT file.
"br" stands for either "browse" or "branches".
The output format is similar to that of the common `tree` program.

To illustrate the program's capabilities, shown below are the listings it produces for the several of the
ROOT tutorial files.
```
> rootbr $ROOTSYS/tutorials/hsimple.root
TH1F hpx
└── TPaveStats stats
TH2F hpxpy
TProfile hprof
TNtuple ntuple [25,000]
├── Float_t px
├── Float_t py
├── Float_t pz
├── Float_t random
└── Float_t i
```
```
> rootbr $ROOTSYS/tutorials/dataframe/df017_vecOpsHEP.root
TTree myDataset [3]
├── Int_t nPart
├── Double_t px: px[nPart]
├── Double_t py: py[nPart]
└── Double_t E: E[nPart]
```
```
> rootbr -S $ROOTSYS/tutorials/quadp/stock.root
File size: 160.03 kB

Warning in <TClass::Init>: no dictionary for class TStockDaily is available
TTree GE [974]
└── TStockDaily daily
    ├── Int_t fDate
    ├── Int_t fOpen
    ├── Int_t fHigh
    ├── Int_t fLow
    ├── Int_t fClose
    ├── Int_t fVol
    └── Int_t fCloseAdj
# Remaining output omitted here for brevity
```

The program can print types and names of all object in a ROOT file, recursively traversing the directory structure.
By default, it also lists types and names of `TTree` branches.
This was the main motivation for writing this program, as it allows to forgo the use of `TBrowser` and to
easily inspect `TFile` and `TTree` contents in combination with standard Unix and GNU utilities, such as `grep`.
For example, `rootbr file.root | grep -i weight` is a convenient way to find all weight branches in an ntuple and
get their types.

If you find this program useful, I recommend adding `alias br='rootbr'` to your `.bashrc` or its equivalent.

# Program options
```
usage: rootbr [options...] file.root [objects...]
* Short options can be passed multiple times to toggle the behavior
  -b           [off ] histograms' binning
  -B           [on  ] TTree branches
  -c           [auto] color output
  -d [depth]   [0   ] directory traversal (0 = all)
  -I           [off ] histograms' integrals
  -p           [off ] use Print() for objects and ls() for directories
  -s           [off ] sort listed items
  -S           [off ] file size
  -t           [off ] objects' titles
  -T           [off ] objects' timestamps
  --ls         call TFile::ls()
  --map        call TFile::Map()
  --streamer   call TFile::ShowStreamerInfo()
  -h, --help   display this help text and exit
```

[`getopt()`](https://man7.org/linux/man-pages/man3/getopt.3.html) is used to parse the program options.

By default, if you are on a Unix-like system, the output is colored if printing to a terminal,
but otherwise, e.g. when stdout is piped, the output is not colored, similarly to the behavior of `ls --color=auto`.
Option `-c` toggles between always and never using colored output.
For example, a large amount of output can be conveniently viewed in color with `rootbr -c file.root | less -R`.
`rootbr -cc file.root` will produce uncolored output.

The `-d` option, taking an optional unsigned integer argument, allows to limit the depth of traversal of directories.
`0` depth means that all subdirectories will be printed.
`1` means only the direct contents of the input `TFile` are printed.
Without the argument, `-d` toggles the value between `0` and `1`.

Additional arguments after the file name are interpreted as names of objects to be inspected.
In case at least one object name is provided, only the contents of these objects will be printed.
This is useful for inspecting specific directories in a large directory tree or for looking at
specific `TTree`s in a file that contains multiple.

# ROOT Forum discussion
[LINK](https://root-forum.cern.ch/t/terminal-program-for-printing-root-file-contents-in-a-tree-format/44185?u=ivankp)

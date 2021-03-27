# rootbr
This is a terminal program that can be used to quickly and easily explore the contents of a CERN ROOT file.
"br" stands for either "browse" or "branches".
The output format is based on that of a common `tree` utility.
For example:
```
TDirectoryFile weight2
├── TDirectoryFile all
│   ├── TDirectoryFile all
│   └── TDirectoryFile with_photon_cuts
├── TDirectoryFile gg
│   ├── TDirectoryFile all
│   └── TDirectoryFile with_photon_cuts
├── TDirectoryFile gq
│   ├── TDirectoryFile all
│   └── TDirectoryFile with_photon_cuts
└── TDirectoryFile qq
    ├── TDirectoryFile all
    └── TDirectoryFile with_photon_cuts
```

The program can print types and names of all object in a ROOT file, recursively traversing the directory structure.
By default, it also lists types and names of `TTree` branches.
This was the main motivation for writing this program, as it allows to forgo the use of `TBrowser` and to
easily inspect `TFile` and `TTree` contents in combination with standard Unix and GNU utilities, such as `grep`.
For example, `rootbr file.root | grep -i weight` is a convenient way to find all weight branches in an ntuple and
get their types.

By default, if you are on a Unix-like system, the output is colored if printing to a terminal, but otherwise is not,
similarly to the behavior of `ls --color=auto`.
Option `-c` (lowercase) forces the output to always be colored.
Option `-C` (uppercase) forces the output to never be colored.
For example, a large amount of output can be conveniently viewed in color with `rootbr -c file.root | less -R`.

If you find this program useful, I recommend adding `alias br='rootbr -s'` to your `.bashrc` or its equivalent.

# Program options
```
usage: rootbr [options...] file.root [objects...]
  -b           print histograms' binning
  -c           force color output
  -C           don't color output
  -d           max directory depth
  -i           print histograms' integrals
  -p           use Print() or ls()
  -s           print file size
  -t           print objects' titles
  -T           don't print TTree branches
  --ls         call TFile::ls()
  --map        call TFile::Map()
  --streamer   call TFile::ShowStreamerInfo()
  -h, --help   display this help text and exit
  ```

Additional arguments after the file name are interpreted as names of objects to be inspected.
In case at least one object name is provided, only the contents of these objects will be printed.
This is useful for instpecting specific directories in a large directory tree or for looking at
specific `TTree`s in a file that contains multiple.

The `-d` option, taking a numeric argument, allows to limit the depth of traversal of directories.

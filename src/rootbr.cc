// Program for printing contents of CERN ROOT files
// Written by Ivan Pogrebnyak
// ivan.pogrebnyak@cern.ch
// https://github.com/ivankp/rootbr

#include <iostream>
#include <sstream>
#include <vector>
#include <locale>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include <TFile.h>
#include <TTree.h>
#include <TKey.h>
#include <TBranch.h>
#include <TBranchElement.h>
#include <TBranchSTL.h>
#include <TBranchObject.h>
#include <TLeaf.h>
#include <TH1.h>

#ifdef __has_include
#  if __has_include(<sys/stat.h>)
#    define HAS_SYS_STAT_H
#    include <sys/stat.h>
#  else
#    include <fstream>
#  endif
#  if __has_include(<unistd.h>)
#    define HAS_UNISTD_H
#    include <unistd.h>
#  endif
#endif

using std::cout;
using std::cerr;
using std::endl;

enum opt_val : uint8_t { opt_false=0, opt_true=1, opt_auto=2 };

opt_val opt_c = // color
#ifdef HAS_UNISTD_H
  opt_auto;
#else
  opt_false;
#endif
bool opt_s = false, // print file size
     opt_t = false, // print objects' titles
     opt_b = false, // print histograms' binning
     opt_i = false, // print histograms' integrals
     opt_p = false, // use Print() for specified objects
     opt_T = false, // don't print TTree branches
     opt_map = false,
     opt_ls = false,
     opt_streamer = false;

int max_depth = 0;

double file_size(const char* name) {
#ifdef HAS_SYS_STAT_H
  struct stat st;
  if (stat(name,&st)==-1) throw std::runtime_error(std::strerror(errno));
  return st.st_size;
#else
  std::ifstream f;
  f.exceptions(std::ifstream::failbit);
  f.open(name, std::ifstream::ate | std::ifstream::binary);
  return in.tellg();
#endif
}
void print_file_size(const char* name) {
  double size = file_size(name);
  unsigned i = 0;
  for ( ; size > 1024 && i < 5; ++i) size /= 1024;
  std::stringstream ss;
  ss << "File size: ";
  ss.precision(2);
  ss << std::fixed << size << ' ' << " kMGT"[i] << "B\n\n";
  cout << ss.rdbuf();
  cout.flush();
}

#define non_empty_cmp(a,b) a && *a && strcmp(a,b)

template <typename T>
bool inherits_from(TClass* c) {
  return c->InheritsFrom(T::Class());
}

std::vector<bool> indent;
void print_indent(bool last) {
  if (const unsigned n = indent.size()-1) {
    indent.back() = last;
    for (unsigned i=1;; ++i) {
      const bool l = indent[i];
      if (i == n) {
        cout << (l ? "└── " : "├── ");
        break;
      } else {
        cout << (l ? "    " : "│   ");
      }
    }
  }
}
void print_indent_prop(bool sub=false) {
  for (unsigned n=indent.size(), i=1; i<n; ++i)
    cout << (indent[i] ? "    " : "│   ");
  cout << (sub ? "│   " : "    ");
}

void print(
  const char* class_name, const char* name, const char* color_str
) {
  if (class_name) {
    if (opt_c) cout << color_str;
    cout << class_name;
    if (opt_c) cout << "\033[0m";
    cout << ' ';
  }
  if (name) cout << name;
}
void print(
  const char* class_name, const char* name, const char* color_str,
  Short_t cycle
) {
  print(class_name,name,color_str);
  if (cycle!=1) {
    if (opt_c) cout << "\033[2;37m;";
    cout << cycle;
    if (opt_c) cout << "\033[0m";
  }
}

void print_branch(
  const char* class_name, const char* name, const char* color_str,
  const char* title
) {
  print(class_name, name, color_str);
  if (non_empty_cmp(title,name)) {
    if (opt_c) cout << "\033[2;37m";
    cout << ": " << title;
    if (opt_c) cout << "\033[0m";
  }
  cout << '\n';
}

void print(TBranch* b, bool last);

void print_branch(TBranch* b, bool last) {
  const char* const bname = b->GetName();
  TObjArray* branches = b->GetListOfBranches();
  const Int_t nbranches = branches->GetEntriesFast();
  TObjArray* const leaves = b->GetListOfLeaves();
  TLeaf* const last_leaf = static_cast<TLeaf*>(leaves->Last());
  const char* const lname = last_leaf->GetName();
  const char* const btitle = b->GetTitle();
  if (nbranches) {
    print_indent(false);
    print_branch(b->GetClassName(), bname, "\033[1;35m",
      strcmp(btitle,lname) ? btitle : nullptr);
    if (b->GetNleaves() != 1 || strcmp(bname,lname)) {
      indent.emplace_back();
      for (TObject* l : *leaves) {
        print_indent(l==last_leaf);
        print_branch(
          static_cast<TLeaf*>(l)->GetTypeName(),
          static_cast<TLeaf*>(l)->GetName(),
          "\033[35m",
          static_cast<TLeaf*>(l)->GetTitle()
        );
      }
      indent.pop_back();
    }
    for (Int_t i=0; i<nbranches; i++)
      print(static_cast<TBranch*>(branches->At(i)), nbranches-i==1);
  } else {
    print_indent(last);
    if (b->GetNleaves() == 1 && !strcmp(bname,lname)) {
      print_branch(
        last_leaf->GetTypeName() ?: b->GetClassName(),
        bname,
        "\033[35m",
        last_leaf->GetTitle()
      );
    } else {
      print_branch(
        b->GetClassName(),
        bname,
        "\033[35m",
        btitle
      );

      indent.emplace_back();
      for (TObject* l : *leaves) {
        print_indent(l==last_leaf);
        print_branch(
          static_cast<TLeaf*>(l)->GetTypeName(),
          static_cast<TLeaf*>(l)->GetName(),
          "\033[35m",
          static_cast<TLeaf*>(l)->GetTitle()
        );
      }
      indent.pop_back();
    }
  }
}

void print(TBranch* b, bool last) {
  if (dynamic_cast<TBranchElement*>(b)) {
    print_branch(b,last);
  } else if (dynamic_cast<TBranchSTL*>(b)) {
    print_branch(b,last);
  } else if (dynamic_cast<TBranchObject*>(b)) {
    print_branch(b,last);
  } else {
    print_indent(last);
    const char* const bname = b->GetName();

    TObjArray* const leaves = b->GetListOfLeaves();
    TLeaf* const last_leaf = static_cast<TLeaf*>(leaves->Last());

    if (b->GetNleaves() == 1 && !strcmp(bname,last_leaf->GetName())) {
      print_branch(
        last_leaf->GetTypeName(),
        bname,
        "\033[35m",
        last_leaf->GetTitle()
      );
    } else {
      print_branch(
        b->GetClassName(),
        bname,
        "\033[35m",
        b->GetTitle()
      );

      indent.emplace_back();
      for (TObject* l : *leaves) {
        print_indent(l==last_leaf);
        print_branch(
          static_cast<TLeaf*>(l)->GetTypeName(),
          static_cast<TLeaf*>(l)->GetName(),
          "\033[35m",
          static_cast<TLeaf*>(l)->GetTitle()
        );
      }
      indent.pop_back();
    }
  }
}

void print(TTree* tree) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << tree->GetEntries();
  cout << " [" << ss.rdbuf() << "]\n";

  TList* aliases = tree->GetListOfAliases();
  const bool has_aliases = aliases && aliases->GetEntries() > 0;

  TObjArray* const branches = tree->GetListOfBranches();
  TObject* const last_branch = branches->Last();

  if (opt_t) {
    const char* title = tree->GetTitle();
    if (non_empty_cmp(title,tree->GetName())) {
      print_indent_prop(branches->GetEntries() > 0 || has_aliases);
      cout << title << '\n';
    }
  }
  indent.emplace_back();

  for (TObject* b : *branches)
    print(static_cast<TBranch*>(b), b == last_branch && !has_aliases);

  if (has_aliases) {
    TObject* const last = aliases->Last();
    for (TObject* alias : *aliases) {
      print_indent(alias == last);
      const char* name = alias->GetName();
      cout << name;
      cout << (opt_c ? " \033[36m->\033[0m " : " -> ");
      cout << tree->GetAlias(name) << '\n';
    }
  }
  indent.pop_back();
}

void print(TH1* hist);

void print(TList* list, bool keys=true) {
  indent.emplace_back();
  TObject* const last = list->Last();
  for (TObject* item : *list) {
    const char* const name = item->GetName();
    const char* class_name;
    Short_t cycle = 1;
    if (keys) {
      class_name = static_cast<TKey*>(item)->GetClassName();
      cycle = static_cast<TKey*>(item)->GetCycle();
    } else {
      class_name = item->ClassName();
    }

    print_indent(item == last);

    TClass* const class_ptr = TClass::GetClass(class_name,true,true);
    if (!class_ptr) {
      print(class_name,name,"\033[1;31m",cycle);
      cout << '\n';
    } else if (inherits_from<TTree>(class_ptr)) {
      print(class_name,name,"\033[1;32m",cycle);
      if (!opt_T) {
        if (keys) item = static_cast<TKey*>(item)->ReadObj();
        print(static_cast<TTree*>(item));
      } else cout << '\n';
    } else if (inherits_from<TDirectory>(class_ptr)) {
      print(class_name,name,"\033[1;34m",cycle);
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth) {
        if (keys) item = static_cast<TKey*>(item)->ReadObj();
        print(static_cast<TDirectory*>(item)->GetListOfKeys());
      }
    } else if (inherits_from<TH1>(class_ptr)) {
      print(class_name,name,"\033[34m",cycle);
      if (keys) item = static_cast<TKey*>(item)->ReadObj();
      print(static_cast<TH1*>(item));
    } else {
      print(class_name,name,"\033[34m",cycle);
      if (opt_t) {
        if (keys) item = static_cast<TKey*>(item)->ReadObj();
        const char* title = item->GetTitle();
        if (non_empty_cmp(title,name)) {
          cout << '\n';
          print_indent_prop();
          cout << "    " << title;
        }
      }
      cout << '\n';
    }
  }
  indent.pop_back();
}

void print_hist_binning(TH1* h, bool sub) {
  std::stringstream ss;
  const TAxis* axes[3] { };
  const int na = h->GetDimension();
  if (na>0) axes[0] = h->GetXaxis();
  if (na>1) axes[1] = h->GetYaxis();
  if (na>2) axes[2] = h->GetZaxis();
  ss.precision(std::numeric_limits<double>::max_digits10);
  for (int i=0; i<na; ++i) {
    print_indent_prop(sub);
    ss << "xyz"[i] << ": ";
    const TAxis* const a = axes[i];
    if (i) ss << ',';
    const auto* bins = a->GetXbins();
    if (int n = bins->GetSize()) {
      const auto* b = bins->GetArray();
      ss << '[';
      for (int i=0; i<n; ++i) {
        if (i) ss << ',';
        ss << b[i];
      }
      ss << ']';
    } else {
      n = a->GetNbins();
      ss << '('
         << n << ", "
         << a->GetXmin() << ", "
         << a->GetXmax()
         << ')';
    }
  }
  cout << ss.rdbuf() << '\n';
}

void print(TH1* hist) {
  cout << '\n';
  TList* fcns = hist->GetListOfFunctions();
  const bool has_fcns = fcns && fcns->GetEntries() > 0;
  if (opt_t) {
    const char* title = hist->GetTitle();
    if (non_empty_cmp(title,hist->GetName())) {
      print_indent_prop(has_fcns);
      cout << title << '\n';
    }
  }
  if (opt_b)
    print_hist_binning(hist,has_fcns);
  if (opt_i) {
    print_indent_prop(has_fcns);
    cout << "∫: " << hist->Integral(0,-1) << '\n';
  }
  print(fcns,false);
}

void print(TObject* obj) {
  const char* const class_name = obj->ClassName();
  const char* const name = obj->GetName();
  if (TTree* p = dynamic_cast<TTree*>(obj)) {
    print(class_name,name,"\033[1;32m");
    if (!opt_T) {
      indent.emplace_back();
      print(p);
      indent.pop_back();
    }
  } else if (TDirectory* p = dynamic_cast<TDirectory*>(obj)) {
    print(class_name,name,"\033[1;34m");
    cout << '\n';
    indent.emplace_back();
    if (max_depth==0 || (int)indent.size() < max_depth)
      print(p->GetListOfKeys());
    indent.pop_back();
  } else if (TH1* p = dynamic_cast<TH1*>(obj)) {
    print(class_name,name,"\033[34m");
    indent.emplace_back();
    print(p);
    indent.pop_back();
  } else {
    print(class_name,name,"\033[34m");
    if (opt_t) {
      const char* title = obj->GetTitle();
      if (non_empty_cmp(title,name)) {
        cout << '\n';
        print_indent_prop();
        cout << "    " << title;
      }
    }
    cout << '\n';
  }
}

void print_usage(const char* prog) {
  cout << "usage: " << prog <<
    " [options...] file.root [objects...]\n"
#ifdef HAS_UNISTD_H
    "  -b           print histograms' binning\n"
    "  -c           force color output\n"
    "  -C           don't color output\n"
    "  -d           max directory depth\n"
    "  -i           print histograms' integrals\n"
    "  -p           use Print() or ls()\n"
    "  -s           print file size\n"
    "  -t           print objects' titles\n"
    "  -T           don't print TTree branches\n"
#endif
    "  --ls         call TFile::ls()\n"
    "  --map        call TFile::Map()\n"
    "  --streamer   call TFile::ShowStreamerInfo()\n"
    "  -h, --help   display this help text and exit\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  for (int i=1; i<argc; ) {
    const char* arg = argv[i];
    if (*(arg++)=='-' && *(arg++)=='-') {
      if (!strcmp(arg,"help")) {
        print_usage(argv[0]);
        return 0;
      } else if (!strcmp(arg,"ls")) { opt_ls = true;
      } else if (!strcmp(arg,"map")) { opt_map = true;
      } else if (!strcmp(arg,"streamer")) { opt_streamer = true;
      } else {
        cerr << "invalid option --" << arg << '\n';
        return 1;
      }
      --argc;
      for (int j=i; j<argc; ++j)
        argv[j] = argv[j+1];
    } else ++i;
  }
#ifdef HAS_UNISTD_H
  for (int o; (o = getopt(argc,argv,"hcCstbipTd:")) != -1; ) {
    switch (o) {
      case 'c': opt_c = opt_true;  break;
      case 'C': opt_c = opt_false; break;
      case 's': opt_s = true; break;
      case 't': opt_t = true; break;
      case 'b': opt_b = true; break;
      case 'i': opt_i = true; break;
      case 'p': opt_p = true; break;
      case 'T': opt_T = true; break;
      case 'd':
        if ((max_depth = atoi(optarg)) <= 0) {
          cerr << "-d: depth argument must be a positive number\n";
          return 1;
        }
        break;
      case 'h': print_usage(argv[0]); return 0;
      default : return 1;
    }
  }
  if (!(optind<argc)) {
    print_usage(argv[0]);
    return 1;
  }
  if (opt_c==opt_auto) opt_c = isatty(1) ? opt_true : opt_false;
#else
  int optind = 1;
#endif
  const char* fname = argv[optind];
  ++optind;

  if (opt_s) {
    try {
      print_file_size(fname);
    } catch (const std::exception& e) {
      cerr << "Failed to open file \"" << fname << "\"\n" << e.what() << '\n';
      return 1;
    }
  }

  TFile file(fname);
  if (file.IsZombie()) return 1;

  if (opt_map || opt_ls || opt_streamer) {
    if (opt_ls) file.ls();
    if (opt_map) file.Map();
    if (opt_streamer) file.ShowStreamerInfo();
  } else if (optind==argc) {
    if (opt_p) file.ls();
    else print(file.GetListOfKeys());
  } else {
    bool first = true;
    for (; optind<argc; ++optind) {
      if (first) first = false;
      else cout << '\n';
      const char* objname = argv[optind];
      TObject* obj = file.Get(objname);
      if (!obj) {
        cerr << "Cannot get object \"" << objname << "\"\n";
        return 1;
      }
      if (opt_p) {
        if (auto* p = dynamic_cast<TDirectory*>(obj)) p->ls();
        else obj->Print();
      } else print(obj);
    }
  }
}

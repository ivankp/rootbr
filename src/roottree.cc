#include <iostream>
#include <sstream>
#include <vector>
#include <locale>
#include <stdexcept>
#include <cstring>

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
     opt_i = false; // print histograms' integrals

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
        cout << (l ? "└" : "├") << "── ";
        break;
      } else {
        cout << (l ? " " : "│") << "   ";
      }
    }
  }
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
  if (title && std::strcmp(name,title)) {
    if (opt_c) cout << "\033[2;37m";
    cout << ": " << title;
    if (opt_c) cout << "\033[0m";
  }
  cout << '\n';
}

void print(TBranch* b);

void print_branch(TBranch* b) {
  const char* const bname = b->GetName();
  TObjArray* branches = b->GetListOfBranches();
  const Int_t nbranches = branches->GetEntriesFast();
  TObjArray* const leaves = b->GetListOfLeaves();
  TLeaf* const last_leaf = static_cast<TLeaf*>(leaves->Last());
  const char* const lname = last_leaf->GetName();
  const char* const btitle = b->GetTitle();
  if (nbranches) {
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
    const bool last = indent.back();
    for (Int_t i=0; i<nbranches; i++) {
      print_indent(last && nbranches-i==1);
      print(static_cast<TBranch*>(branches->At(i)));
    }
  } else {
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

void print(TBranch* b) {
  if (dynamic_cast<TBranchElement*>(b)) {
    print_branch(b);
  } else if (dynamic_cast<TBranchSTL*>(b)) {
    print_branch(b);
  } else if (dynamic_cast<TBranchObject*>(b)) {
    print_branch(b);
  } else {
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

  indent.emplace_back();

  TList* aliases = tree->GetListOfAliases();
  const bool has_aliases = aliases && aliases->GetEntries() > 0;

  TObjArray* const branches = tree->GetListOfBranches();
  TObject* const last_branch = branches->Last();

  for (TObject* b : *branches) {
    print_indent(b == last_branch && !has_aliases);
    print(static_cast<TBranch*>(b));
  }

  if (has_aliases) {
    TObject* const last = aliases->Last();
    for (TObject* alias : *aliases) {
      print_indent(alias == last);
      const char* name = alias->GetName();
      cout << name << " -> " << tree->GetAlias(name) << '\n';
    }
  }
  indent.pop_back();
}

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
      if (keys) item = static_cast<TKey*>(item)->ReadObj();
      print(static_cast<TTree*>(item));
    } else if (inherits_from<TDirectory>(class_ptr)) {
      print(class_name,name,"\033[1;34m",cycle);
      cout << '\n';
      if (keys) item = static_cast<TKey*>(item)->ReadObj();
      print(static_cast<TDirectory*>(item)->GetListOfKeys());
    } else if (inherits_from<TH1>(class_ptr)) {
      print(class_name,name,"\033[34m",cycle);
      cout << '\n';
      if (keys) item = static_cast<TKey*>(item)->ReadObj();
      TH1 *h = static_cast<TH1*>(item);
      print(h->GetListOfFunctions(),false);
    } else {
      print(class_name,name,"\033[34m",cycle);
      cout << '\n';
    }
  }
  indent.pop_back();
}

// Options:
// - show histogram title
// - show histogram binning
// - show histogram integral
// - opt_c: auto, always, never

void print_usage(const char* prog) {
  cout << "usage: " << prog <<
#ifdef HAS_UNISTD_H
    " [OPTION]... file.root\n"
    "  -c           force color output\n"
    "  -C           do not color output\n"
    "  -s           print file size\n"
    "  -t           print objects' titles\n"
    "  -b           print histograms' binning\n"
    "  -i           print histograms' integrals\n"
    "  -h, --help   display this help text and exit\n";
#else
    " file.root\n";
#endif
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
#ifdef HAS_UNISTD_H
  for (int i=1; i<argc; ++i) {
    if (!strcmp(argv[i],"--help")) {
      print_usage(argv[0]);
      return 0;
    }
  }
  for (int o; (o = getopt(argc, argv, "hcCstbi")) != -1; ) {
    switch (o) {
      case 'c': opt_c = opt_true;  break;
      case 'C': opt_c = opt_false; break;
      case 's': opt_s = true; break;
      case 't': opt_t = true; break;
      case 'b': opt_b = true; break;
      case 'i': opt_i = true; break;
      case 'h': print_usage(argv[0]); return 0;
      default :
        cerr << "Unknown option ";
        if (isprint(optopt))
          cerr << '-' << char(optopt) << '\n';
        else
          cerr << "character " << std::hex << char(optopt) << '\n';
        return 1;
    }
  }
  if (!(optind<argc)) {
    print_usage(argv[0]);
    return 1;
  }
  if (opt_c==opt_auto) opt_c = isatty(1) ? opt_true : opt_false;
  const char* fname = argv[optind];
#else
  const char* fname = argv[1];
#endif

  try {
    print_file_size(fname);
  } catch (const std::exception& e) {
    if (opt_c) cerr << "\033[31m";
    cerr << "Failed to open file \"" << fname << "\"\n" << e.what();
    if (opt_c) cerr << "\033[0m";
    cerr << '\n';
    return 1;
  }

  TFile file(fname);
  if (file.IsZombie()) return 1;

  print(file.GetListOfKeys());
}

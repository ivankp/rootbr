// Program for printing contents of CERN ROOT files
// Written by Ivan Pogrebnyak
// ivan.pogrebnyak@cern.ch
// https://github.com/ivankp/rootbr

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <locale>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <charconv>

#include <TFile.h>
#include <TKey.h>
#include <TTree.h>
#include <TBranch.h>
#include <TBranchElement.h>
#include <TBranchSTL.h>
#include <TBranchObject.h>
#include <TLeaf.h>
#include <TFolder.h>
#include <TH1.h>
#include <TPad.h>

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

#define STR1(x) #x
#define STR(x) STR1(x)
#define CAT1(a,b) a ## b
#define CAT(a,b) CAT1(a,b)

using std::cout;
using std::cerr;
using std::endl;

enum opt_val : uint8_t { opt_false=0, opt_true=1, opt_auto=2 };

#define OPT_INIT_b 0
#define OPT_INIT_B 1
#define OPT_INIT_d 0
#define OPT_INIT_I 0
#define OPT_INIT_p 0
#define OPT_INIT_s 0
#define OPT_INIT_S 0
#define OPT_INIT_t 0
#define OPT_INIT_T 0

#ifdef HAS_UNISTD_H
#define OPT_INIT_c 2
#else
#define OPT_INIT_c 0
#endif

opt_val opt_c = static_cast<opt_val>(OPT_INIT_c);
bool
  opt_b = OPT_INIT_b,
  opt_B = OPT_INIT_B,
  opt_I = OPT_INIT_I,
  opt_p = OPT_INIT_p,
  opt_s = OPT_INIT_s,
  opt_S = OPT_INIT_S,
  opt_t = OPT_INIT_t,
  opt_T = OPT_INIT_T,
  opt_map = false,
  opt_ls = false,
  opt_streamer = false;

int max_depth = 0;

#define TOGGLE(x) x = !x
#define non_empty_cmp(a,b) a && *a && strcmp(a,b)

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

[[gnu::const]]
char to_upper(char c) noexcept {
  return (c < 'a' || 'z' < c) ? c : c-(char)32;
}

enum : char { NUMB, LETT, SYMB, CTRL, EXTD };

[[gnu::const]]
char char_cat(char c) noexcept {
  if (c <   '\0') return EXTD;
  if (c <    ' ') return CTRL;
  if (c <    '0') return SYMB;
  if (c <    ':') return NUMB;
  if (c <    'A') return SYMB;
  if (c <    '[') return LETT;
  if (c < '\x7F') return SYMB;
  return CTRL;
}

bool lex_str_less(std::string_view a, std::string_view b) noexcept {
  const char* s1 = a.data();
  const char* s2 = b.data();
  const char* const z1 = s1 + a.size();
  const char* const z2 = s2 + b.size();

  for (char c1, c2; s1!=z1 && s2!=z2; ) {
    c1 = to_upper(*s1);
    c2 = to_upper(*s2);

    const char t1 = char_cat(c1);
    const char t2 = char_cat(c2);
    if (t1 != t2) return t1 < t2;

    if (t1 == NUMB) {
      unsigned u1=0, u2=0;
      s1 = std::from_chars(s1,z1,u1).ptr;
      s2 = std::from_chars(s2,z2,u2).ptr;
      if (u1 != u2) return u1 < u2;
      continue;
    } else if (c1 != c2) return c1 < c2;

    ++s1, ++s2;
  }

  return (z1-s1) < (z2-s2);
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
  Short_t cycle, const char* t
) {
  print(class_name,name,color_str);
  const bool c = opt_c && ( (cycle!=1) || t );
  if (c) cout << "\033[2;37m";
  if (cycle!=1) cout << ';' << cycle;
  if (t) cout << ' ' << t;
  if (c) cout << "\033[0m";
}

// ******************************************************************
void print(TObject*);
void print(TKey*);
void print(TCollection*);
void print(TTree*);
void print(TBranch*, bool last);
void print(TH1*);
// ******************************************************************

void print(TObject* obj) {
  const char* const name = obj->GetName();

  if (auto* key = dynamic_cast<TKey*>(obj)) {
    const Short_t cycle = key->GetCycle();
    const char* const class_name = key->GetClassName();
    TClass* const class_ptr = TClass::GetClass(class_name,true,true);
    const char* t = opt_T ? key->GetDatime().AsSQLString() : nullptr;

    if (!class_ptr) {
      print(class_name,name,"\033[1;31m",cycle,t);
      cout << '\n';

    } else if (inherits_from<TDirectory>(class_ptr)) {
      print(class_name,name,"\033[1;34m",cycle,t);
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(dynamic_cast<TDirectory*>(key->ReadObj())->GetListOfKeys());

    } else if (inherits_from<TFolder>(class_ptr)) {
      print(class_name,name,"\033[1;34m",cycle,t);
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(dynamic_cast<TFolder*>(key->ReadObj())->GetListOfFolders());

    } else if (inherits_from<TTree>(class_ptr)) {
      print(class_name,name,"\033[1;32m",cycle,t);
      if (opt_B) {
        print(dynamic_cast<TTree*>(key->ReadObj()));
      } else cout << '\n';

    } else if (inherits_from<TH1>(class_ptr)) {
      print(class_name,name,"\033[34m",cycle,t);
      print(dynamic_cast<TH1*>(key->ReadObj()));

    } else if (inherits_from<TPad>(class_ptr)) {
      print(class_name,name,"\033[34m",cycle,t);
      cout << '\n';
      print(dynamic_cast<TPad*>(key->ReadObj())->GetListOfPrimitives());

    } else if (inherits_from<TCollection>(class_ptr)) {
      print(class_name,name,"\033[1;34m",cycle,t);
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(dynamic_cast<TCollection*>(key->ReadObj()));

    } else {
      print(class_name,name,"\033[34m",cycle,t);
      if (opt_t) {
        const char* title = key->ReadObj()->GetTitle();
        if (non_empty_cmp(title,name)) {
          cout << '\n';
          print_indent_prop();
          cout << "    " << title;
        }
      }
      cout << '\n';

    }

  } else {
    const char* const class_name = obj->ClassName();

    if (auto* p = dynamic_cast<TDirectory*>(obj)) {
      print(class_name,name,"\033[1;34m");
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(p->GetListOfKeys());

    } else if (auto* p = dynamic_cast<TFolder*>(obj)) {
      print(class_name,name,"\033[1;34m");
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(p->GetListOfFolders());

    } else if (auto* p = dynamic_cast<TTree*>(obj)) {
      print(class_name,name,"\033[1;32m");
      if (opt_B)
        print(p);

    } else if (auto* p = dynamic_cast<TH1*>(obj)) {
      print(class_name,name,"\033[34m");
      print(p);

    } else if (auto* p = dynamic_cast<TPad*>(obj)) {
      print(class_name,name,"\033[34m");
      cout << '\n';
      print(p->GetListOfPrimitives());

    } else if (auto* p = dynamic_cast<TCollection*>(obj)) {
      print(class_name,name,"\033[1;34m");
      cout << '\n';
      if (max_depth==0 || (int)indent.size() < max_depth)
        print(p);

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
}

template <typename T, typename F>
void for_items(const T& items, F&& f) {
  auto it = items.begin();
  const auto end = items.end();
  if (it != end) {
    for (;;) {
      TObject* item = *it;
      const bool last = ++it == end;
      f(item,last);
      if (last) break;
    }
  }
}
template <typename F>
void for_coll(const TCollection& coll, F&& f) {
  if (opt_s) {
    std::vector<TObject*> items;
    items.reserve(coll.GetEntries());
    for (TObject* item : coll)
      items.push_back(item);
    std::stable_sort(items.begin(),items.end(),
      [](const TObject* a, const TObject* b){
        return lex_str_less(a->GetName(),b->GetName());
      });
    for_items(items,std::forward<F>(f));
  } else {
    for_items(coll,std::forward<F>(f));
  }
}

void print(TCollection* coll) {
  indent.emplace_back();
  for_coll(*coll,[](TObject* item, bool last){
    print_indent(last);
    print(item);
  });
  indent.pop_back();
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

void print(TBranch* b, bool last) {
  const char* const bname = b->GetName();

  TObjArray* const leaves = b->GetListOfLeaves();
  const Int_t nleaves = b->GetNleaves();
  TLeaf* const leaf =
    nleaves==1 ? static_cast<TLeaf*>(leaves->First()) : nullptr;

  TObjArray* branches = b->GetListOfBranches();
  if (branches && branches->GetEntriesFast() <= 0) branches = nullptr;

  print_indent(last);
  bool indented = false;
  if (leaf && !strcmp(bname,leaf->GetName())) {
    const char* type = leaf->GetTypeName();
    if (!type || !*type) type = b->GetClassName();
    print_branch(type, bname, "\033[35m", leaf->GetTitle());
  } else {
    print_branch(b->GetClassName(), bname, "\033[35m", b->GetTitle());
    if (nleaves > 0) {
      indent.emplace_back();
      indented = true;
      for_coll(*leaves,[nb=!branches](TObject* leaf, bool last){
        print_indent(last && nb);
        print_branch(
          static_cast<TLeaf*>(leaf)->GetTypeName(),
          static_cast<TLeaf*>(leaf)->GetName(),
          "\033[32m",
          static_cast<TLeaf*>(leaf)->GetTitle()
        );
      });
    }
  }

  if (branches) {
    if (!indented) {
      indent.emplace_back();
      indented = true;
    }
    for_coll(*branches,[](TObject* b, bool last){
      print(static_cast<TBranch*>(b), last);
    });
  }
  if (indented) indent.pop_back();
}

void print(TTree* tree) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << tree->GetEntries();
  cout << " [" << ss.rdbuf() << "]\n";

  TObjArray* const branches = tree->GetListOfBranches();

  TList* aliases = tree->GetListOfAliases();
  if (aliases && aliases->GetEntries() <= 0) aliases = nullptr;

  if (opt_t) {
    const char* title = tree->GetTitle();
    if (non_empty_cmp(title,tree->GetName())) {
      print_indent_prop(branches->GetEntries() > 0 || aliases);
      cout << title << '\n';
    }
  }
  indent.emplace_back();

  for_coll(*branches,[na=!aliases](TObject* b, bool last){
    print(static_cast<TBranch*>(b), last && na);
  });

  if (aliases)
    for_coll(*aliases,[tree](TObject* alias, bool last){
      print_indent(last);
      const char* name = alias->GetName();
      cout << name;
      cout << (opt_c ? " \033[36m->\033[0m " : " -> ");
      cout << tree->GetAlias(name) << '\n';
    });

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
  if (opt_I) {
    print_indent_prop(has_fcns);
    cout << "∫: " << hist->Integral(0,-1) << '\n';
  }
  print(fcns);
}

TObject* get_object(TDirectory* dir, char* name) {
  while (*name == '/') ++name;
  for (char* p; (p = strchr(name,'/')); ) {
    *p = '\0';
    dir = dynamic_cast<TDirectory*>(dir->Get(name));
    *p = '/';
    if (!dir) return nullptr;
    while (*++p == '/') ;
    name = p;
    if (!*name) return dir;
  }
  return dir->Get(name);
}

#define INIT_VAL_STR_0 "off "
#define INIT_VAL_STR_1 "on  "
#define INIT_VAL_STR_2 "auto"
#define OPT_INIT_VAL_STR(o) "[" CAT(INIT_VAL_STR_,CAT(OPT_INIT_,o)) "]"

void print_usage(const char* prog) {
  cout << "usage: " << prog << " [options...] file.root [objects...]\n"
    "* Short options can be passed multiple times to toggle the behavior\n"
#ifdef HAS_UNISTD_H
    "  -b           " OPT_INIT_VAL_STR(b) " histograms' binning\n"
    "  -B           " OPT_INIT_VAL_STR(B) " TTree branches\n"
    "  -c           " OPT_INIT_VAL_STR(c) " color output\n"
    "  -d [depth]   [" STR(OPT_INIT_d) "   ]"
      " directory traversal (0 = all)\n"
    "  -I           " OPT_INIT_VAL_STR(I) " histograms' integrals\n"
    "  -p           " OPT_INIT_VAL_STR(p)
      " use Print() for objects and ls() for directories\n"
    "  -s           " OPT_INIT_VAL_STR(s) " sort listed items\n"
    "  -S           " OPT_INIT_VAL_STR(S) " file size\n"
    "  -t           " OPT_INIT_VAL_STR(t) " objects' titles\n"
    "  -T           " OPT_INIT_VAL_STR(T) " objects' timestamps\n"
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
      } else if (*arg) {
        cerr << argv[0] << ": invalid option --" << arg << '\n';
        return 1;
      }
      --argc;
      for (int j=i; j<argc; ++j)
        argv[j] = argv[j+1];
    } else ++i;
  }
#ifdef HAS_UNISTD_H
  opterr = 0;
  for (int o; (o = getopt(argc,argv,":hbBcd:IpsStT")) != -1; ) {
    switch (o) {
      case 'h': print_usage(argv[0]); return 0;
      case 'b': TOGGLE(opt_b); break;
      case 'B': TOGGLE(opt_B); break;
      case 'c': opt_c = (opt_c==opt_true ? opt_false : opt_true); break;
      case 'I': TOGGLE(opt_I); break;
      case 'p': TOGGLE(opt_p); break;
      case 's': TOGGLE(opt_s); break;
      case 'S': TOGGLE(opt_S); break;
      case 't': TOGGLE(opt_t); break;
      case 'T': TOGGLE(opt_T); break;
      case 'd':
        if (optarg[0]=='-' && *std::find(argv,argv+argc,optarg)) {
          TOGGLE(max_depth);
          --optind;
        } else if ([]{
          const char* s = optarg;
          for (char c; (c = *s); ++s)
            if (c < '0' || '9' < c) return true;
          return false;
        }()) {
          cerr << argv[0]
            << ": option -d argument must be a nonnegative integer\n";
          return 1;
        } else max_depth = atoi(optarg);
        break;
      case ':':
        switch (optopt) {
          case 'd': TOGGLE(max_depth); break;
          default :
            cerr << argv[0] << ": option -" << (char)optopt
              << " requires an argument\n";
            return 1;
        }
        break;
      case '?':
        cerr << argv[0] << ": invalid option -";
        if (optopt > 0) cerr << (char)optopt;
        else cerr << '\'' << optopt << '\'';
        cerr << '\n';
      default :
        return 1;
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

  if (opt_S) {
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
    indent.emplace_back();
    for (; optind<argc; ++optind) {
      if (first) first = false;
      else cout << '\n';
      char* objname = argv[optind];
      TObject* obj = get_object(&file,objname);
      if (!obj) {
        cerr << "Cannot get object \"" << objname << "\"\n";
        return 1;
      }
      if (opt_p) {
        if (auto* p = dynamic_cast<TDirectory*>(obj)) p->ls();
        else obj->Print();
      } else print(obj);
    }
    indent.pop_back();
  }
}

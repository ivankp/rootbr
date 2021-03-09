#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <locale>
#include <stdexcept>
#include <cstring>

#include <TFile.h>
#include <TTree.h>
#include <TKey.h>
#include <TBranch.h>
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

#ifdef HAS_UNISTD_H
bool color = true;
#else
bool color = false;
#endif

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T>
  bool operator()(const T& a, const char* b) const noexcept {
    return a < b;
  }
  template <typename T>
  bool operator()(const char* a, const T& b) const noexcept {
    return a < b;
  }
};

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
  if (color) cout << color_str;
  cout << class_name;
  if (color) cout << "\033[0m";
  cout << ' ' << name;
}
void print(
  const char* class_name, const char* name, const char* color_str,
  Short_t cycle
) {
  print(class_name,name,color_str);
  if (cycle!=1) {
    if (color) cout << "\033[90m;";
    cout << cycle;
    if (color) cout << "\033[0m";
  }
}
void print_branch(
  const char* class_name, const char* name, const char* color_str,
  const char* title
) {
  print(class_name, name, color_str);
  if (title && std::strcmp(name,title)) {
    cout << ": ";
    if (color) cout << "\033[90m";
    cout << title;
    if (color) cout << "\033[0m";
  }
  cout << '\n';
}

void print(TTree* tree) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << tree->GetEntries();
  cout << " [" << ss.rdbuf() << "]\n";

  std::set<const char*,chars_less> bnames;

  indent.emplace_back();
  TObjArray* const branches = tree->GetListOfBranches();
  TObject* const last_branch = branches->Last();
  TList* aliases = tree->GetListOfAliases();
  const bool has_aliases = aliases && aliases->GetEntries() > 0;
  for (TObject* b : *branches) {
    const char* const bname = static_cast<TBranch*>(b)->GetName();
    const bool dup = !bnames.emplace(bname).second;

    TObjArray* const leaves = static_cast<TBranch*>(b)->GetListOfLeaves();
    TLeaf* const last_leave = static_cast<TLeaf*>(leaves->Last());

    print_indent(b == last_branch && !has_aliases);

    if (
      static_cast<TBranch*>(b)->GetNleaves() == 1 &&
      !strcmp(bname,last_leave->GetName())
    ) {
      print_branch(
        last_leave->GetTypeName(),
        last_leave->GetName(),
        dup ? "\033[31m" : "\033[35m",
        last_leave->GetTitle()
      );
    } else {
      print_branch(
        static_cast<TBranch*>(b)->GetClassName(),
        bname,
        dup ? "\033[31m" : "\033[35m",
        static_cast<TBranch*>(b)->GetTitle()
      );

      indent.emplace_back();
      for (TObject* l : *leaves) {
        print_indent(l==last_leave);
        print_branch(
          static_cast<TLeaf*>(l)->GetTypeName(),
          static_cast<TLeaf*>(l)->GetName(),
          "\033[35m",
          static_cast<TLeaf*>(l)->GetTitle()
        );
      } // end leaf loop
      indent.pop_back();
    }
  } // end branch loop

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
      print(class_name,name,"\033[31m",cycle);
      cout << '\n';
    } else {
      if (keys) item = static_cast<TKey*>(item)->ReadObj();
      if (inherits_from<TTree>(class_ptr)) {
        print(class_name,name,"\033[1;92m",cycle);
        print(static_cast<TTree*>(item));
      } else if (inherits_from<TDirectory>(class_ptr)) {
        print(class_name,name,"\033[1;34m",cycle);
        cout << '\n';
        print(static_cast<TDirectory*>(item)->GetListOfKeys());
      } else if (inherits_from<TH1>(class_ptr)) {
        print(class_name,name,"\033[34m",cycle);
        cout << '\n';
        TH1 *h = static_cast<TH1*>(item);
        print(h->GetListOfFunctions(),false);
      } else {
        print(class_name,name,"\033[34m",cycle);
        cout << '\n';
      }
    }
  }
  indent.pop_back();
}

// Options:
// - show histogram title
// - show histogram binning
// - show histogram integral
// - color: auto, always, never

int main(int argc, char** argv) {
  if (argc < 2) {
    cout << "usage: " << argv[0] << " file.root\n";
    return 1;
  }
#ifdef HAS_UNISTD_H
  color = isatty(1);
#endif

  try {
    print_file_size(argv[1]);
  } catch (const std::exception& e) {
    if (color) cerr << "\033[31m";
    cerr << "Failed to open file \"" << argv[1] << "\"\n" << e.what();
    if (color) cerr << "\033[0m";
    cerr << '\n';
    return 1;
  }

  TFile file(argv[1]);
  if (file.IsZombie()) return 1;

  print(file.GetListOfKeys());
}

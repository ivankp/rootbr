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

#ifdef HAS_UNISTD_H
bool color = true;
#else
bool color = false;
#endif

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
    if (color) cout << color_str;
    cout << class_name;
    if (color) cout << "\033[0m";
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
    if (color) cout << "\033[2;37m;";
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
    if (color) cout << "\033[2;37m";
    cout << ": " << title;
    if (color) cout << "\033[0m";
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
      print(class_name,name,"\033[1;92m",cycle);
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

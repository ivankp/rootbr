.PHONY: all clean install

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CPPFLAGS := -Iinclude
CXXFLAGS := -Wall -O3 -flto -fmax-errors=3
# CXXFLAGS := -Wall -g -fmax-errors=3

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

ROOT_CPPFLAGS := $(shell root-config --cflags)
ROOT_PREFIX   := $(shell root-config --prefix)
ROOT_LIBDIR   := $(shell root-config --libdir)
ROOT_LDFLAGS  := $(shell root-config --ldflags) -Wl,-rpath,$(ROOT_LIBDIR)
ROOT_LDLIBS   := $(shell root-config --libs)

EXE := bin/rootbr

all: $(EXE)

C_rootbr := $(ROOT_CPPFLAGS)
LF_rootbr := $(ROOT_LDFLAGS)
L_rootbr := -L$(ROOT_LIBDIR) -lCore -lRIO -lTree -lHist -lGpad

#####################################################################

.PRECIOUS: .build/%.o

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.cc,$^) -o $@

-include $(shell [ -d .build ] && find .build -type f -name '*.d')

install: all
	@: "$${PREFIX:=$(ROOT_PREFIX)}"; \
	 for f in $(EXE); do \
	   install -m 755 -vD "$$f" "$$PREFIX/$$f"; \
	 done

endif ###############################################################

clean:
	@rm -rfv bin .build


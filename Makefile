######################################################################
# How to use this Makefile
#
# make		-- build optimized executable in opt/
# make gprof	-- build executable for profiling with gprof in gprof/
# make debug	-- build executable for debugging with gdb in debug/
# make clean	-- clean generated files for optimized version
# make cleanall	-- clean generated files for all above versions, and
#                  dependency files.
######################################################################

######################################################################
# Project specific settings
#
# Assumptions: *nix system, GNU c++ compiler is used
#
# Variables:
#   TARGET
#   SRCDIR
#   SRCS
#   OPTS
######################################################################

TARGET=clcheck
SRCDIR=.
SRCS=main.cpp parser.cpp solver.cpp

# 32-bit target for comparison with vercheck
OPTS=-Wall -m32


######################################################################
# Reusable definitions below
######################################################################

# default options
TARGET?=a.out
SRCDIR?=.
SRCS?=main.cpp

# the name of the C++ compiler
GPP?=g++

# the default profile
PROFILE?=release

# set OPTS according to version
ifeq ($(PROFILE),debug)
    OPTS += -O0 -ggdb -DDEBUG -D_GLIBCXX_DEBUG
    #also: -DDEBUG_HOLES -DDEBUG_DEFS
else
  ifeq ($(PROFILE),gprof)
    OPTS += -O3 -pg
  else
    # optimized versions
    OPTS += -O3
  endif
endif

DIR=build/$(PROFILE)
DEPDIR=$(DIR)/deps

DEPS=$(patsubst %.cpp, $(DEPDIR)/%.d, $(SRCS))
OBJS=$(patsubst %.cpp, $(DIR)/%.o, $(SRCS))

.PHONY: release gprof debug clean clean2 cleanall

all: $(DIR)/$(TARGET)

release:	
	$(MAKE) PROFILE=release

gprof:	
	$(MAKE) PROFILE=gprof

debug:
	$(MAKE) PROFILE=debug

$(DEPDIR):
	mkdir -p $(DEPDIR)

$(DEPS): $(DEPDIR)

$(DEPDIR)/%.d : $(SRCDIR)/%.cpp
	$(GPP) $(OPTS) -MM -MF $@ -MT $(DIR)/$*.o -c $<

$(DIR)/%.o : $(SRCDIR)/%.cpp
	$(GPP) $(OPTS) -o $@ -c $<

$(DIR)/$(TARGET): $(DEPDIR) $(DEPS) $(OBJS)
	$(GPP) $(OPTS) -o $@ $(OBJS)

clean:
	rm -f $(DIR)/$(TARGET) $(OBJS)

cleanall:
	$(MAKE) clean
	$(MAKE) PROFILE=gprof clean
	$(MAKE) PROFILE=debug clean

-include $(DEPS)


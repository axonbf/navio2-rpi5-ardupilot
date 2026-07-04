CXX ?= g++
CFLAGS = -std=gnu++11
LDFLAGS = -lnavio -lrt -lpthread -lgpiod

LIBDIR= $(CURDIR)/Navio
SOURCEDIR = $(CURDIR)/Examples
BUILDDIR = $(CURDIR)/Build

INCLUDES = -I$(LIBDIR)
LIB_INCLUDES = -L$(LIBDIR)

MODULES = $(wildcard $(SOURCEDIR)/*.cpp)
EXECUTABLES = $(patsubst %, $(BUILDDIR)/%, $(notdir $(MODULES:.cpp=)))

.PHONY: all lib clean

all: $(EXECUTABLES)

lib:
	$(MAKE) -C $(LIBDIR) all

$(BUILDDIR)/%: $(SOURCEDIR)/%.cpp lib
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) $< $(LIB_INCLUDES) $(LDFLAGS) -o $@
		
clean:
	$(MAKE) -C $(LIBDIR) clean
	rm -rf $(BUILDDIR)

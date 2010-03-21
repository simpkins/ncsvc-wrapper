#
# Installation paths
#

NC_DIR = $(HOME)/.juniper_networks/network_connect
INSTALL_DIR = $(NC_DIR)/rtwrap

#
# Build configuration
#

CC = gcc
CFLAGS = -g -O2 -std=c99 -Wall -Wextra -Werror -fPIC
CPPFLAGS = '-DNC_DIR="$(NC_DIR)"' '-DINSTALL_DIR="$(INSTALL_DIR)"'

INSTALL = install
LN_S = ln -s

#
# Targets
#

all: librtwrap.so ncsvc_wrap

clean:
	$(RM) librtwrap.so ncsvc_wrap *.o

install:
	@if test ! -f '$(NC_DIR)/ncsvc'; then \
	    echo '"$(NC_DIR)" does not appear to contain a Network Connect' \
		'installation' >&2 ; \
	    exit 1; \
	fi
	$(INSTALL) -d '$(INSTALL_DIR)'
	$(INSTALL) -t '$(INSTALL_DIR)' librtwrap.so
	$(INSTALL) -t '$(INSTALL_DIR)' -o root -m 6711 ncsvc_wrap
	@if test ! -L '$(NC_DIR)/ncsvc'; then \
	    echo 'Backing up original ncsvc binary, and setting up symlink'; \
	    mv '$(NC_DIR)/ncsvc' '$(INSTALL_DIR)/ncsvc.orig'; \
	    $(LN_S) '$(INSTALL_DIR)/ncsvc_wrap' '$(NC_DIR)/ncsvc'; \
	fi

librtwrap.so: rtwrap.o
	$(CC) $(LDFLAGS) -m32 -shared -o $@ $^ -ldl

rtwrap.o: rtwrap.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -m32 -fPIC -c -o $@ $<

ncsvc_wrap: ncsvc_wrap.o
	$(CC) $(LDFLAGS) -o $@ $^

ncsvc_wrap.o: ncsvc_wrap.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

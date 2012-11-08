#
# Makefile for Linux force feedback effect creation and testing tool, based
# on the Makefile for Linux input utilities.
#
# © 2011-2012 Stephen Kitt <steve@sk2.org>
# Copyright (C) 2012 Jolla Ltd.
# Contact: Kalle Jokiniemi <kalle.jokiniemi@jollamobile.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA.

VERSION := 0.4

PACKAGE := fftune-$(VERSION)

CFLAGS		= -g -O2 -Wall -I../linux/include

PREFIX          ?= /usr/local

PROGRAMS	= fftune

all: $(PROGRAMS)

distclean: clean
	$(RM) -rf $(PACKAGE)*
clean:
	$(RM) *.o *.swp $(PROGRAMS) *.orig *.rej map *~ rpm/*~

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin

dist: clean
	rm -rf $(PACKAGE)
	mkdir $(PACKAGE)
	cp -a fftune.c Makefile $(PACKAGE)
	(cd $(PACKAGE); find . -name .svn -o -name *~ | xargs rm -rf)
	tar cjf $(PACKAGE).tar.bz2 $(PACKAGE)
	rm -rf $(PACKAGE)

.PHONY: all clean distclean install dist

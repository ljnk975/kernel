# --------------------------------------------------- -*- Makefile -*- --
# $Id: Makefile,v 1.3 2012/11/27 00:48:37 phil Exp $
#
# @Copyright@
# 
# 				Rocks(r)
# 		         www.rocksclusters.org
# 		         version 6.2 (SideWinder)
# 		         version 7.0 (Manzanita)
# 
# Copyright (c) 2000 - 2017 The Regents of the University of California.
# All rights reserved.	
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
# notice unmodified and in its entirety, this list of conditions and the
# following disclaimer in the documentation and/or other materials provided 
# with the distribution.
# 
# 3. All advertising and press materials, printed or electronic, mentioning
# features or use of this software must display the following acknowledgement: 
# 
# 	"This product includes software developed by the Rocks(r)
# 	Cluster Group at the San Diego Supercomputer Center at the
# 	University of California, San Diego and its contributors."
# 
# 4. Except as permitted for the purposes of acknowledgment in paragraph 3,
# neither the name or logo of this software nor the names of its
# authors may be used to endorse or promote products derived from this
# software without specific prior written permission.  The name of the
# software includes the following terms, and any derivatives thereof:
# "Rocks", "Rocks Clusters", and "Avalanche Installer".  For licensing of 
# the associated name, interested parties should contact Technology 
# Transfer & Intellectual Property Services, University of California, 
# San Diego, 9500 Gilman Drive, Mail Code 0910, La Jolla, CA 92093-0910, 
# Ph: (858) 534-5815, FAX: (858) 534-7345, E-MAIL:invent@ucsd.edu
# 
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# @Copyright@
#
# $Log: Makefile,v $
# Revision 1.3  2012/11/27 00:48:37  phil
# Copyright Storm for Emerald Boa
#
# Revision 1.2  2012/05/06 05:48:43  phil
# Copyright Storm for Mamba
#
# Revision 1.1  2012/02/01 20:42:59  phil
# Setuptools needed for MYSQL-python build on Rocks v. 6
#
#

PKGROOT		= /opt/rocks
REDHAT.ROOT     = $(CURDIR)/../../
HTTPGET         = $(ROCKSROOT)/bin/httpget.sh
-include $(ROCKSROOT)/etc/Rules.mk
include Rules.mk


build:
# ROCKS8: Need to relocate from foundation-python-extras due to pygtk build dependencies
# ROCKS8: Bump to version 1.56.1 (same as on Rocky8)
# https://download.gnome.org/sources/gobject-introspection/1.56/gobject-introspection-1.56.1.tar.xz
ifeq ($(shell ! test -f $(PYPKGNAME)-$(VERSION).tar.xz && echo -n yes),yes)
	@echo "ROCKS8: Sideloading $(PYPKGNAME)-$(VERSION).tar.gz."
	$(HTTPGET) -B https://download.gnome.org -F sources/gobject-introspection/1.56 -n $(PYPKGNAME)-$(VERSION).tar.xz
endif
	unxz -c $(PYPKGNAME)-$(VERSION).tar.xz | $(TAR) -xf -
	(									\
		cd $(PYPKGNAME)-$(VERSION);					\
		PATH=/opt/rocks/bin:$$PATH ./configure --prefix=$(PKGROOT);	\
		$(MAKE);							\
	)

install::
	(									\
		cd $(PYPKGNAME)-$(VERSION);					\
		$(MAKE) prefix=$(ROOT)/$(PKGROOT) install;			\
		mkdir -p $(ROOT)/$(PKGROOT)/share/gir-1.0;			\
		install -c -m 644 gir/DBus-1.0.gir gir/DBusGLib-1.0.gir gir/fontconfig-2.0.gir gir/freetype2-2.0.gir gir/GL-1.0.gir gir/libxml2-2.0.gir gir/xft-2.0.gir gir/xlib-2.0.gir gir/xfixes-4.0.gir gir/xrandr-1.3.gir gir/win32-1.0.gir gir/cairo-1.0.gir GLib-2.0.gir GObject-2.0.gir GModule-2.0.gir Gio-2.0.gir GIRepository-2.0.gir $(ROOT)/$(PKGROOT)/share/gir-1.0;	\
	)

clean::
	rm -rf $(PYPKGNAME)-$(VERSION)

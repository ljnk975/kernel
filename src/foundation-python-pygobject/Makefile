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


build::
# ROCKS8: Need to relocate from foundation-python-extras due to pygtk build dependencies
# https://gitlab.gnome.org/GNOME/pygobject/-/blob/master/NEWS: 3.36.1 is last 
# release supporting Python 2.
# https://download.gnome.org/sources/pygobject/3.36/pygobject-3.36.1.tar.xz
# Note 3.36 throws an error with foundation-blivet:
# AttributeError: 'gi.repository.BlockDev' object has no attribute 'plugin_specs_from_names'
# Trying last 2.x version (also same on Rocky8)
# https://download.gnome.org/sources/pygobject/2.28/pygobject-2.28.7.tar.xz
# Same error - rrot cause were missing python-blockdev bindings. Switching back to 3.36.1, because
# pygobject-2.28.7 does not compile with gobject-introspection
#
# Trying pygobject-2.28.7 again with following simple patch due to problems with
# foundation-NetworkManager https://mail-index.netbsd.org/pkgsrc-users/2012/02/08/msg015675.html
#
# Again switching back to Centos7 Version https://download.gnome.org/sources/pygobject/3.22/pygobject-3.22.0.tar.xz
# due to anaconda problems with
# File "/opt/rocks/lib/python2.7/site-packages/gi/overrides/GLib.py", line 99, in _create_tuple
#    builder = GLib.VariantBuilder()
# MemoryError
# 3.22 does not compile! 3.29 compiles.
ifeq ($(shell ! test -f $(PYPKGNAME)-$(VERSION).tar.gz && echo -n yes),yes)
	@echo "ROCKS9: Sideloading $(PYPKGNAME)-$(VERSION).tar.xz."
	$(HTTPGET) -B https://download.gnome.org -F sources/pygobject/2.28 -n $(PYPKGNAME)-$(VERSION).tar.xz
endif
	unxz -c $(PYPKGNAME)-$(VERSION).tar.xz | $(TAR) -xf -
	-cd patch-files && find . -type f | grep -v CVS | cpio -pduv ../
	(									\
		cd $(PYPKGNAME)-$(VERSION);					\
		PYTHON=$(PY.PATH)						\
		PKG_CONFIG_PATH=/opt/rocks/lib/pkgconfig:$$PKG_CONFIG_PATH 	\
		LD_LIBRARY_PATH=/opt/rocks/lib \
		./configure --prefix=$(PKGROOT) --enable-introspection;		\
		$(MAKE);							\
	)

install::
	mkdir -p $(ROOT)/$(PKGROOT)
	(								\
		cd $(PYPKGNAME)-$(VERSION);				\
		$(MAKE) prefix=$(ROOT)/$(PKGROOT) install;		\
	)

clean::
	rm -rf $(PYPKGNAME)-$(VERSION)

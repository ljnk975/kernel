SRCPKGHOST 	= https://src.fedoraproject.org
SRCPKGPATH 	= repo/pkgs/libxklavier/libxklavier-5.4.tar.bz2/13af74dcb6011ecedf1e3ed122bd31fa
NAME		= foundation-libxklavier
PKGROOT		= /opt/rocks
VERSION		= 5.4
RELEASE		= 0
SUBDIR		= libxklavier-$(VERSION)
TARFILE		= $(SUBDIR).tar.bz2
RPM.FILES	= "$(PKGROOT)/include/*\\n$(PKGROOT)/lib/*\\n$(PKGROOT)/share/*"

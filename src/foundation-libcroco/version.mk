SRCPKGHOST 	= https://download.gnome.org
SRCPKGPATH 	= sources/libcroco/0.6
NAME		= foundation-libcroco
PKGROOT		= /opt/rocks
VERSION		= 0.6.12
RELEASE		= 0
SUBDIR		= libcroco-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.FILES	= "$(PKGROOT)/bin/*\\n$(PKGROOT)/lib/*\\n$(PKGROOT)/include/**\\n$(PKGROOT)/share/*"
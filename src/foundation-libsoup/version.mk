SRCPKGHOST 	= https://download.gnome.org
SRCPKGPATH 	= sources/libsoup/2.50
NAME		= foundation-libsoup
PKGROOT		= /opt/rocks
VERSION		= 2.50.0
RELEASE		= 0
SUBDIR		= libsoup-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.FILES	= "$(PKGROOT)/lib/*\\n$(PKGROOT)/include/*\\n$(PKGROOT)/share/*"
SRCPKGHOST 	= https://download.gnome.org
SRCPKGPATH 	= sources/librsvg/2.42
NAME		= foundation-librsvg
PKGROOT		= /opt/rocks
VERSION		= 2.42.9
RELEASE		= 0
SUBDIR		= librsvg-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.FILES	= "$(PKGROOT)/lib/*\\n$(PKGROOT)/share/*\\n$(PKGROOT)/bin/*\\n$(PKGROOT)/include/*"

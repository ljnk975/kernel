NAME		= foundation-glade
PKGROOT		= /opt/rocks
VERSION		= 3.19.0
RELEASE		= 0
SUBDIR		= glade-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.REQUIRES	= foundation-atk,foundation-at-spi2-atk,foundation-glib2,foundation-gtk+,foundation-devel-module
RPM.FILES	= "$(PKGROOT)/bin/*\\n$(PKGROOT)/include/*\\n$(PKGROOT)/lib/*\\n$(PKGROOT)/share/*"

NAME		= foundation-gtk+
PKGROOT		= /opt/rocks
VERSION		= 3.16.7
RELEASE		= 0
SUBDIR		= gtk+-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.REQUIRES	= foundation-atk,foundation-at-spi2-core,foundation-at-spi2-atk,foundation-glib2,foundation-devel-module
RPM.FILES	= "$(PKGROOT)/bin/*\\n$(PKGROOT)/etc/*\\n$(PKGROOT)/include/*\\n$(PKGROOT)/lib/*\\n$(PKGROOT)/share/*"

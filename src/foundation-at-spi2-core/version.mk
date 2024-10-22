NAME		= foundation-at-spi2-core
PKGROOT		= /opt/rocks
VERSION		= 2.26.2
RELEASE		= 0
SUBDIR		= at-spi2-core-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.FILES	= "$(PKGROOT)/include/*\\n$(PKGROOT)/lib/*\\n$(PKGROOT)/etc/*\\n$(PKGROOT)/share/*\\n$(PKGROOT)/libexec/*"

NAME		= foundation-glib2
PKGROOT		= /opt/rocks
VERSION		= 2.62.0
RELEASE		= 0
SUBDIR		= glib-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.REQUIRES	= 
RPM.FILES	= "$(PKGROOT)/bin/*\\n$(PKGROOT)/include/*\\n$(PKGROOT)/lib64/*\\n$(PKGROOT)/share/*"

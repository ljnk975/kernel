SRCPKGHOST 	= https://src.fedoraproject.org
SRCPKGPATH 	= repo/pkgs/libcanberra/libcanberra-0.30.tar.xz/md5/34cb7e4430afaf6f447c4ebdb9b42072
NAME		= foundation-libcanberra
PKGROOT		= /opt/rocks
VERSION		= 0.30
RELEASE		= 0
SUBDIR		= libcanberra-$(VERSION)
TARFILE		= $(SUBDIR).tar.xz
RPM.FILES	= "$(PKGROOT)/lib/*"
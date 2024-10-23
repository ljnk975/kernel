Summary: foundation-python-gobject-introspection
Name: foundation-python-gobject-introspection
Version: 1.56.1
Release: 0
License: University of California
Vendor: Rocks Clusters
Group: System Environment/Base
Source: foundation-python-gobject-introspection-1.56.1.tar.gz
Buildroot: /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.buildroot







%define _build_id_links none
%define __brp_mangle_shebangs %{nil}
%description
foundation-python-gobject-introspection
%prep
%setup
%build
printf "\n\n\n### build ###\n\n\n"
BUILDROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.buildroot make -f /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.spec.mk build
%install
printf "\n\n\n### install ###\n\n\n"
BUILDROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.buildroot make -f /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.spec.mk install
%files 
/opt/rocks/bin/*
/opt/rocks/include/*
/opt/rocks/lib/*
/opt/rocks/share/*


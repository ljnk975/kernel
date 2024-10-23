Summary: foundation-python-pygtk
Name: foundation-python-pygtk
Version: 2.24.0
Release: 0
License: University of California
Vendor: Rocks Clusters
Group: System Environment/Base
Source: foundation-python-pygtk-2.24.0.tar.gz
Buildroot: /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.buildroot







%define _build_id_links none
%define __brp_mangle_shebangs %{nil}
%description
foundation-python-pygtk
%prep
%setup
%build
printf "\n\n\n### build ###\n\n\n"
BUILDROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.buildroot make -f /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.spec.mk build
%install
printf "\n\n\n### install ###\n\n\n"
BUILDROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.buildroot make -f /home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.spec.mk install
%files 
/opt/rocks/bin/*
/opt/rocks/lib/*
/opt/rocks/include/*
/opt/rocks/share/*
/opt/rocks/lib/python2*/site-packages/*


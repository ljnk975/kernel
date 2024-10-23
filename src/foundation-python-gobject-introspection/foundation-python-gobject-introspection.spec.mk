# This file is called from the generated spec file.
# It can also be used to debug rpm building.
# 	make -f foundation-python-gobject-introspection.spec.mk build|install

ifndef __RULES_MK
build:
	make ROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.buildroot build

install:
	make ROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-gobject-introspection/foundation-python-gobject-introspection.buildroot install
endif

# This file is called from the generated spec file.
# It can also be used to debug rpm building.
# 	make -f foundation-python-pygtk.spec.mk build|install

ifndef __RULES_MK
build:
	make ROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.buildroot build

install:
	make ROOT=/home/linhnv57/rocks/src/roll/kernel/src/foundation-python-pygtk/foundation-python-pygtk.buildroot install
endif

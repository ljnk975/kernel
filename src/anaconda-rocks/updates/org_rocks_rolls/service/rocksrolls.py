#
# Copyright (C) 2020 Red Hat, Inc.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions of
# the GNU General Public License v.2, or (at your option) any later version.
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY expressed or implied, including the implied warranties of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.  You should have received a copy of the
# GNU General Public License along with this program; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.  Any Red Hat trademarks that are incorporated in the
# source code or documentation are not subject to the GNU General Public
# License and may only be used or replicated with the express permission of
# Red Hat, Inc.
#
import logging

from pyanaconda.core.configuration.anaconda import conf
from pyanaconda.core.dbus import DBus
from pyanaconda.core.signal import Signal
from pyanaconda.modules.common.base import KickstartService
from pyanaconda.modules.common.containers import TaskContainer

from org_rocks_rolls.constants import ROCKSROLLS
from org_rocks_rolls.service.rocksrolls_interface import RocksRollsInterface
from org_rocks_rolls.service.installation import RocksRollsConfigurationTask, \
    RocksRollsInstallationTask
from org_rocks_rolls.service.kickstart import RocksRollsKickstartSpecification

import subprocess
from org_rocks_rolls import RocksEnv

log = logging.getLogger(__name__)

__all__ = ["RocksRolls"]

class RocksRolls(KickstartService):
    """The RocksRolls D-Bus service.

    This class parses and stores data for the Hello world addon.
    """

    def __init__(self):
        super().__init__()
        self._reverse = False
        self._lines = []

        self.reverse_changed = Signal()
        self.lines_changed = Signal()

    def publish(self):
        """Publish the module."""
        TaskContainer.set_namespace(HELLO_WORLD.namespace)
        DBus.publish_object(HELLO_WORLD.object_path, HelloWorldInterface(self))
        DBus.register_service(HELLO_WORLD.service_name)

    @property
    def kickstart_specification(self):
        """Return the kickstart specification."""
        return HelloWorldKickstartSpecification

    def process_kickstart(self, data):
        """Process the kickstart data."""
        log.debug("Processing kickstart data...")
        self._reverse = data.addons.org_rocks_rolls.reverse
        self._lines = data.addons.org_rocks_rolls.lines

    def setup_kickstart(self, data):
        """Set the given kickstart data."""
        log.debug("Generating kickstart data...")
        data.addons.org_rocks_rolls.reverse = self._reverse
        data.addons.org_rocks_rolls.lines = self._lines

        ## At this point rolls have been selected, the in-memory rocks database
        ## is set up and cluster information has been added by the user. now
        ## need to
        ##   A. add the attributes from data.addons.org_rocks_rolls.info
        ##      into the in-memory database
        ##   B. rolls are in  data.addons.org_rocks_rolls.rolls 
        ##      process the xml files for the rolls specified, then generate
        ##      packages

        ## We don't have "rolls" if we are a clientInstall
        if self.clientInstall or data.addons.org_rocks_rolls.info is None:
            return
        for row in data.addons.org_rocks_rolls.info:
            log.info("ROCKS ADD ATTR %s=%s" % (row[2],row[1]))
            self.addAttr(row[2],row[1])
        cmd = ["/opt/rocks/bin/rocks","report","host","attr","pydict=true"]
        p = subprocess.Popen(cmd,stdout=subprocess.PIPE)
        attrs = p.stdout.readlines()[0].strip()
        
        f = open("/tmp/site.attrs","w")
        atdict = eval(attrs)
        for key in atdict.keys():
            f.write( "%s:%s\n" % (key,atdict[key]) )
        f.close()

        ## Write out all of the Ethernet devices so that we can load
        ## Load into the installed FE database. See database-data.xm
        etherifs = filter(lambda x: nm.nm_device_type_is_ethernet(x),\
           nm.nm_devices())
        ethermacs = map(lambda x: nm.nm_device_valid_hwaddress(x),etherifs)
        g = open("/tmp/frontend-ifaces.sh","w")
        g.write("/opt/rocks/bin/rocks config host interface localhost iface='%s' mac='%s' flag='' module=''\n" % \
            (",".join(etherifs),",".join(ethermacs)))
        g.close()

        cmd = ["/opt/rocks/bin/rocks","list","node", "xml", \
           "attrs=%s" % attrs, "basedir=/tmp/rocks/export/profile", "server"]
        p = subprocess.Popen(cmd,stdout=subprocess.PIPE)
        nodexml = p.stdout.readlines()

        cmd = ["/opt/rocks/sbin/kgen","--section=packages"]
        p = subprocess.Popen(cmd,stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        out,err = p.communicate(input="".join(nodexml))
        pkgs = filter(lambda x: len(x) > 0, out.split("\n")) 

        for pkg in pkgs:
            if "%" in pkg or "#" in pkg:
                continue
            if not pkg in ksdata.packages.packageList:
                ksdata.packages.packageList.append(pkg)

        ## Generate the post scripts section
        log.info("ROCKS GENERATING POST SCRIPTS")
        cmd = ["/opt/rocks/sbin/kgen","--section=post"]
        p = subprocess.Popen(cmd,stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.postscripts,err = p.communicate(input="".join(nodexml))
        log.info("ROCKS POST SCRIPTS GENERATED")

        ksparser = kickstart.AnacondaKSParser(ksdata)
        ksparser.readKickstartFromString(self.postscripts, reset=False)

        ## Add eula and firstboot stanzas 
        log.info("ROCKS FIRSTBOOT/EULA")
        ksparser = kickstart.AnacondaKSParser(ksdata)
        ksparser.readKickstartFromString("eula --agreed", reset=False)
        ksparser.readKickstartFromString("firstboot --disable", reset=False)
        log.info("ROCKS FIRSBOOT/EULA END ")

    @property
    def reverse(self):
        """Whether to reverse order of lines in the hello world file."""
        return self._reverse

    def set_reverse(self, reverse):
        self._reverse = reverse
        self.reverse_changed.emit()
        log.debug("Reverse is set to %s.", reverse)

    @property
    def lines(self):
        """Lines of the hello world file."""
        return self._lines

    def set_lines(self, lines):
        self._lines = lines
        self.lines_changed.emit()
        log.debug("Lines is set to %s.", lines)

    def configure_with_tasks(self):
        """Return configuration tasks.

        The configuration tasks are run at the beginning of the installation process.

        Anaconda's code automatically calls the ***_with_tasks methods and
        stores the returned ***Task instances to later execute their run() methods.
        """
        task = HelloWorldConfigurationTask()
        return [task]

    def install_with_tasks(self):
        """Return installation tasks.

        The installation tasks are run at the end of the installation process.

        Anaconda's code automatically calls the ***_with_tasks methods and
        stores the returned ***Task instances to later execute their run() methods.
        """
        task = HelloWorldInstallationTask(
            conf.target.system_root,
            self._reverse,
            self._lines)
        return [task]

import logging

from dasbus.server.interface import dbus_interface
from dasbus.server.property import emits_properties_changed
from dasbus.typing import *  # pylint: disable=wildcard-import,unused-wildcard-import

from pyanaconda.modules.common.base import KickstartModuleInterface

from org_rocks_rolls.constants import ROCKSROLLS

log = logging.getLogger(__name__)


@dbus_interface(ROCKSROLLS.interface_name)
class RocksRollsInterface(KickstartModuleInterface):
    """
    The interface class is needed for interfacing code running within
    Anaconda's main process and code running in the D-Bus service process. The
    dasbus library will automatically set up a D-Bus interface based on these
    classes.
    """

    def connect_signals(self):
        super().connect_signals()
        self.watch_property("Reverse", self.implementation.reverse_changed)
        self.watch_property("Lines", self.implementation.lines_changed)

    @property
    def Reverse(self) -> Bool:
        """Whether to reverse order of lines in the rocks rolls file."""
        return self.implementation.reverse

    @emits_properties_changed
    def SetReverse(self, reverse: Bool):
        self.implementation.set_reverse(reverse)

    @property
    def Lines(self) -> List[Str]:
        """Lines of the rocks rolls file."""
        return self.implementation.lines

    @emits_properties_changed
    def SetLines(self, lines: List[Str]):
        self.implementation.set_lines(lines)

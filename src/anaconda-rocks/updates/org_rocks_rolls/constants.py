"""This module contains constants that are used by various parts of the addon."""

from dasbus.identifier import DBusServiceIdentifier
from pyanaconda.core.dbus import DBus
from pyanaconda.modules.common.constants.namespaces import ADDONS_NAMESPACE

# These define location of the addon's service on D-Bus. See also the data/*.conf file.

ROCKSROLLS_NAMESPACE = (*ADDONS_NAMESPACE, "RocksRolls")

ROCKSROLLS = DBusServiceIdentifier(
    namespace=ROCKSROLLS_NAMESPACE,
    message_bus=DBus
)

# It's better to store paths without the initial slash "/" because of os.path.join behavior.
ROCKSROLLS_FILE_PATH = "root/hello_world.txt"

#include <ibus.h>

#include "engine.h"

int main(int argc, char **argv)
{
    ibus_init();

    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!conn) {
        g_printerr("Cannot connect to D-Bus session bus\n");
        return 1;
    }

    IBusFactory *factory = ibus_factory_new(conn);
    ibus_factory_add_engine(factory, "ethiopic", IBUS_TYPE_ETHIOPIC_ENGINE);

    if (ibus_get_address()) {
        if (!ibus_bus_request_name(ibus_bus_new(),
                                   "org.freedesktop.IBus.Ethiopic", 0)) {
            g_printerr("Cannot request IBus name\n");
            return 1;
        }
    }

    ibus_main();

    return 0;
}

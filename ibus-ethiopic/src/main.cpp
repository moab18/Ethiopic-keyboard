#include <ibus.h>

#include <cstring>
#include <cstdio>

#include "engine.h"
#include "ethio/logger.h"

#ifndef IBUS_ENGINE_NAME
#error "IBUS_ENGINE_NAME must be defined"
#endif

#ifndef IBUS_COMPONENT_NAME
#define IBUS_COMPONENT_NAME "org.freedesktop.IBus.Ethiopic"
#endif

static void print_engines_xml()
{
    std::printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    std::printf("<engines>\n");
    std::printf("    <engine>\n");
    std::printf("        <name>%s</name>\n", IBUS_ENGINE_NAME);
    std::printf("        <language>am</language>\n");
    std::printf("        <longname>Amharic (SERA)</longname>\n");
    std::printf("        <description>Amharic input using SERA transliteration</description>\n");
    std::printf("        <version>0.1.0</version>\n");
    std::printf("        <license>GPL-3.0+</license>\n");
    std::printf("        <author>Moab</author>\n");
    std::printf("        <icon>ibus-ethiopic</icon>\n");
    std::printf("        <layout>us</layout>\n");
    std::printf("        <rank>0</rank>\n");
    std::printf("        <symbol>&#x1200;</symbol>\n");
    std::printf("    </engine>\n");
    std::printf("</engines>\n");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--xml") == 0) {
            print_engines_xml();
            return 0;
        }
    }

    ethio::logger.enable("/tmp/ethio.log");


    ethio::logger.info("ibus engine starting (pid=%d)", getpid());

    ibus_init();

    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!conn) {
        ethio::logger.warning("Cannot connect to D-Bus session bus");
        g_printerr("Cannot connect to D-Bus session bus\n");
        return 1;
    }

    IBusFactory *factory = ibus_factory_new(conn);
    ibus_factory_add_engine(factory, IBUS_ENGINE_NAME, IBUS_TYPE_ETHIOPIC_ENGINE);

    ethio::logger.info("engine registered: name=%s component=%s",
            IBUS_ENGINE_NAME, IBUS_COMPONENT_NAME);

    if (ibus_get_address()) {
        if (!ibus_bus_request_name(ibus_bus_new(),
                                    IBUS_COMPONENT_NAME, 0)) {
            ethio::logger.warning("Cannot request IBus name: %s",
                    IBUS_COMPONENT_NAME);
            g_printerr("Cannot request IBus name\n");
            return 1;
        }
    }

    ethio::logger.info("entering ibus_main()");
    ibus_main();
    ethio::logger.info("ibus engine terminated");
    return 0;
}

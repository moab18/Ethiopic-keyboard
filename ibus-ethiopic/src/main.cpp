// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#include <ibus.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    std::printf("        <symbol>&#x12A0;&#x121B;</symbol>\n");
    std::printf("    </engine>\n");
    std::printf("</engines>\n");
}

static void ibus_disconnected_cb(IBusBus *bus, gpointer user_data)
{
    ethio::logger.info("ibus daemon disconnected");
    ibus_quit();
}

static void start_engine(gboolean exec_by_ibus)
{
    IBusBus *bus = ibus_bus_new();
    if (!bus) {
        ethio::logger.warning("Cannot create IBusBus");
        g_printerr("Cannot create IBusBus\n");
        std::exit(1);
    }
    g_object_ref_sink(bus);

    if (!ibus_bus_is_connected(bus)) {
        ethio::logger.warning("IBus daemon not reachable via ibus_bus_new, "
                              "falling back to direct session bus");
    }

    g_signal_connect(bus, "disconnected",
                     G_CALLBACK(ibus_disconnected_cb), nullptr);

    GDBusConnection *conn = ibus_bus_get_connection(bus);
    if (!conn) {
        ethio::logger.info("No IBus bus connection, using session bus directly");
        conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        if (!conn) {
            ethio::logger.warning("Cannot connect to D-Bus session bus");
            g_printerr("Cannot connect to D-Bus session bus\n");
            std::exit(1);
        }
    }

    IBusFactory *factory = ibus_factory_new(conn);
    g_object_ref_sink(factory);
    ibus_factory_add_engine(factory, IBUS_ENGINE_NAME,
                            IBUS_TYPE_ETHIOPIC_ENGINE);

    if (exec_by_ibus && ibus_bus_is_connected(bus)) {
        if (!ibus_bus_request_name(bus, IBUS_COMPONENT_NAME, 0)) {
            ethio::logger.warning("Cannot request IBus name: %s",
                                  IBUS_COMPONENT_NAME);
        }
    } else if (!exec_by_ibus && ibus_bus_is_connected(bus)) {
        IBusComponent *component = ibus_component_new(
            IBUS_COMPONENT_NAME,
            "Ethiopic Input Method",
            "0.1.0",
            "GPL-3.0+",
            "Moab",
            "https://github.com/moab18/ethiopic-keyboard",
            LIBEXECDIR "/ibus-engine-ethiopic --ibus",
            "ibus-ethiopic");

        IBusEngineDesc *desc = ibus_engine_desc_new(
            IBUS_ENGINE_NAME,
            "Amharic (SERA)",
            "Amharic input using SERA transliteration",
            "am",
            "GPL-3.0+",
            "Moab",
            "ibus-ethiopic",
            "us");

        ibus_component_add_engine(component, desc);

        ibus_bus_register_component(bus, component);
        ethio::logger.info("engine registered via register_component: %s",
                           IBUS_COMPONENT_NAME);
    }

    ethio::logger.info("entering ibus_main()");
    ibus_main();
    ethio::logger.info("ibus engine terminated");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--xml") == 0) {
            print_engines_xml();
            return 0;
        }
    }

    bool exec_by_ibus = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--ibus") == 0) {
            exec_by_ibus = true;
            break;
        }
    }
    bool log = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--log") == 0) {
            log = true;
            break;
        }
    }
    if (log){
       ethio::logger.enable("/tmp/ethio-" + std::string(g_get_user_name()) + ".log");     
    }
    else{
        ethio::logger.disable();
    }
    ethio::logger.info("ibus engine starting (pid=%d, ibus=%d)",
                       getpid(), exec_by_ibus);

    ibus_init();

    start_engine(exec_by_ibus);

    return 0;
}

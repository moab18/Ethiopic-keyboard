#pragma once

#include <ibus.h>

#include "ethio/engine.h"

#define IBUS_TYPE_ETHIOPIC_ENGINE (ibus_ethiopic_engine_get_type())
#define IBUS_ETHIOPIC_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), IBUS_TYPE_ETHIOPIC_ENGINE, IBusEthiopicEngine))
#define IBUS_ETHIOPIC_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), IBUS_TYPE_ETHIOPIC_ENGINE, IBusEthiopicEngineClass))
#define IBUS_IS_ETHIOPIC_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), IBUS_TYPE_ETHIOPIC_ENGINE))
#define IBUS_IS_ETHIOPIC_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IBUS_TYPE_ETHIOPIC_ENGINE))

GType ibus_ethiopic_engine_get_type(void);

struct IBusEthiopicEnginePrivate {
    ethio::Engine core;
    std::string last_commit;
    std::string last_preedit;
};

typedef struct _IBusEthiopicEngine {
    IBusEngine parent;

    IBusEthiopicEnginePrivate *priv;
    bool is_sensitive_field;
} IBusEthiopicEngine;

typedef struct _IBusEthiopicEngineClass {
    IBusEngineClass parent_class;
} IBusEthiopicEngineClass;

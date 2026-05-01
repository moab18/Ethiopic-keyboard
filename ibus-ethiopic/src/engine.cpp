#include "engine.h"

#include <ibus.h>

#include "ethio/mapping.h"

#ifndef MAPPING_DIR
#define MAPPING_DIR "data"
#endif
#ifndef MAPPING_SOURCE_DIR
#define MAPPING_SOURCE_DIR "data"
#endif

G_DEFINE_TYPE(IBusEthiopicEngine, ibus_ethiopic_engine, IBUS_TYPE_ENGINE)

static ethio::MappingFile shared_mapping;
static bool mapping_loaded = false;

static void ensure_mapping()
{
    if (mapping_loaded) return;

    const char *paths[] = {MAPPING_DIR, MAPPING_SOURCE_DIR, nullptr};
    for (int i = 0; paths[i]; i++) {
        std::string full = std::string(paths[i]) + "/amharic/am-sera-v2.json";
        if (g_file_test(full.c_str(), G_FILE_TEST_EXISTS)) {
            try {
                shared_mapping = ethio::load_mapping_file(full);
                mapping_loaded = true;
                return;
            } catch (const std::exception &e) {
                g_warning("Error loading mapping from %s: %s", full.c_str(), e.what());
            }
        }
    }
    g_warning("Could not find am-sera-v2.json in any search path");
}

static bool has_connection(IBusEthiopicEngine *self)
{
    return ibus_service_get_connection(IBUS_SERVICE(self)) != nullptr;
}

static void commit(IBusEthiopicEngine *self)
{
    std::string text = self->priv->core.flush();
    if (text.empty()) return;

    self->priv->last_commit = text;

    if (!has_connection(self)) return;

    IBusText *ibus_text = ibus_text_new_from_static_string(text.c_str());
    ibus_engine_commit_text(IBUS_ENGINE(self), ibus_text);
}

static void preedit_update(IBusEthiopicEngine *self)
{
    std::string_view composing = self->priv->core.composing();

    self->priv->last_preedit = std::string(composing);

    if (!has_connection(self)) return;

    if (composing.empty()) {
        ibus_engine_hide_preedit_text(IBUS_ENGINE(self));
        return;
    }

    IBusText *text = ibus_text_new_from_static_string(composing.data());
    text->attrs = ibus_attr_list_new();
    ibus_attr_list_append(text->attrs,
        ibus_attr_underline_new(IBUS_ATTR_UNDERLINE_SINGLE, 0,
                                static_cast<guint>(composing.size())));
    ibus_engine_update_preedit_text_with_mode(IBUS_ENGINE(self), text,
                                              static_cast<guint>(composing.size()),
                                              TRUE,
                                              IBUS_ENGINE_PREEDIT_COMMIT);
}

static gboolean
ibus_ethiopic_engine_process_key_event(IBusEngine *engine,
                                       guint keyval,
                                       guint keycode,
                                       guint state)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);

    if (state & IBUS_RELEASE_MASK) return FALSE;

    if (state & IBUS_MOD4_MASK) return FALSE;

    if (self->is_password_field) return FALSE;

    if ((state & IBUS_CONTROL_MASK) &&
        (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R)) {
        self->priv->core.toggle_passthrough();
        self->priv->core.reset();
        commit(self);
        preedit_update(self);
        return TRUE;
    }

    if (self->priv->core.passthrough()) return FALSE;

    switch (keyval) {
    case IBUS_KEY_Escape:
        self->priv->core.reset();
        preedit_update(self);
        return FALSE;

    case IBUS_KEY_BackSpace: {
        std::string_view comp = self->priv->core.composing();
        if (!comp.empty()) {
            self->priv->core.reset();
            preedit_update(self);
            return TRUE;
        }
        return FALSE;
    }

    case IBUS_KEY_Return:
    case IBUS_KEY_KP_Enter:
        commit(self);
        preedit_update(self);
        return FALSE;

    default:
        break;
    }

    gunichar uc = ibus_keyval_to_unicode(keyval);
    if (uc == 0) return FALSE;

    char utf8[7] = {};
    gint len = g_unichar_to_utf8(uc, utf8);
    std::string key(utf8, static_cast<size_t>(len));

    bool handled = self->priv->core.filter(key);

    commit(self);
    preedit_update(self);

    return handled ? TRUE : FALSE;
}

static void
ibus_ethiopic_engine_set_content_type(IBusEngine *engine,
                                      guint purpose,
                                      guint hints)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    self->is_password_field = (purpose == IBUS_INPUT_PURPOSE_PASSWORD);
}

static void
ibus_ethiopic_engine_focus_out(IBusEngine *engine)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    if (self->priv->core.passthrough()) {
        self->priv->core.toggle_passthrough();
    }
    self->priv->core.reset();
    preedit_update(self);
}

static void
ibus_ethiopic_engine_reset(IBusEngine *engine)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    self->priv->core.reset();
    preedit_update(self);
}

static void
ibus_ethiopic_engine_init(IBusEthiopicEngine *self)
{
    self->priv = new IBusEthiopicEnginePrivate();
    self->is_password_field = false;

    ensure_mapping();
    if (!shared_mapping.states.empty()) {
        self->priv->core.load_trie(shared_mapping.states[0].trie);
    }
}

static void
ibus_ethiopic_engine_finalize(GObject *obj)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(obj);
    delete self->priv;
    self->priv = nullptr;
    G_OBJECT_CLASS(ibus_ethiopic_engine_parent_class)->finalize(obj);
}

static void
ibus_ethiopic_engine_class_init(IBusEthiopicEngineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

    gobject_class->finalize = ibus_ethiopic_engine_finalize;

    engine_class->process_key_event = ibus_ethiopic_engine_process_key_event;
    engine_class->set_content_type = ibus_ethiopic_engine_set_content_type;
    engine_class->focus_out = ibus_ethiopic_engine_focus_out;
    engine_class->reset = ibus_ethiopic_engine_reset;
}


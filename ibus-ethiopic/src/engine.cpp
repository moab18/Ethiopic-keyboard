#include "engine.h"

#include <ibus.h>

#include "ethio/logger.h"
#include "ethio/mapping.h"
#include "ethio/json.hpp"

#include <fstream>

#ifndef MAPPING_DIR
#define MAPPING_DIR "data"
#endif
#ifndef MAPPING_SOURCE_DIR
#define MAPPING_SOURCE_DIR "data"
#endif

G_DEFINE_TYPE(IBusEthiopicEngine, ibus_ethiopic_engine, IBUS_TYPE_ENGINE)

static ethio::MappingFile shared_mapping;
static ethio::WordList shared_wordlist;
static bool mapping_loaded = false;

static std::string find_data_file(const std::string &relative_path)
{
    const char *paths[] = {MAPPING_DIR, MAPPING_SOURCE_DIR, nullptr};
    for (int i = 0; paths[i]; i++) {
        std::string full = std::string(paths[i]) + "/" + relative_path;
        if (g_file_test(full.c_str(), G_FILE_TEST_EXISTS))
            return full;
    }
    return "";
}

static void ensure_mapping()
{
    if (mapping_loaded) return;

    ethio::logger.debug("ensure_mapping: MAPPING_DIR=%s MAPPING_SOURCE_DIR=%s",
            MAPPING_DIR, MAPPING_SOURCE_DIR);

    std::string mapping_path = find_data_file("amharic/am-sera.json");
    if (!mapping_path.empty()) {
        ethio::logger.debug("ensure_mapping: loading mapping from %s",
                mapping_path.c_str());
        try {
            shared_mapping = ethio::load_mapping_file(mapping_path);
            mapping_loaded = true;
            ethio::logger.debug("ensure_mapping: loaded %zu state(s) "
                    "input_method=%s title=%s",
                    shared_mapping.states.size(),
                    shared_mapping.input_method.c_str(),
                    shared_mapping.title.c_str());
        } catch (const std::exception &e) {
            ethio::logger.warning("Error loading mapping: %s", e.what());
        }
    } else {
        ethio::logger.warning("Could not find am-sera.json in any search path");
        return;
    }

    std::string wordlist_path = find_data_file("amharic/wordlist.json");
    if (!wordlist_path.empty()) {
        ethio::logger.debug("ensure_mapping: loading wordlist from %s",
                wordlist_path.c_str());
        try {
            shared_wordlist.load(wordlist_path);
            ethio::logger.debug("ensure_mapping: loaded %zu words",
                    shared_wordlist.size());
        } catch (const std::exception &e) {
            ethio::logger.warning("Error loading wordlist: %s", e.what());
        }
    } else {
        ethio::logger.debug("ensure_mapping: no wordlist.json found");
    }

    std::string bigrams_path = find_data_file("amharic/bigrams.json");
    if (!bigrams_path.empty()) {
        ethio::logger.debug("ensure_mapping: loading bigrams from %s",
                bigrams_path.c_str());
        try {
            shared_wordlist.load_bigrams(bigrams_path);
            ethio::logger.debug("ensure_mapping: loaded bigrams");
        } catch (const std::exception &e) {
            ethio::logger.warning("Error loading bigrams: %s", e.what());
        }
    } else {
        ethio::logger.debug("ensure_mapping: no bigrams.json found");
    }

    std::string names_path = find_data_file("amharic/names.json");
    if (!names_path.empty()) {
        ethio::logger.debug("ensure_mapping: loading names from %s",
                names_path.c_str());
        try {
            std::ifstream nfs(names_path);
            ethio::Json names_root;
            nfs >> names_root;
            const auto &states = names_root["states"];
            int count = 0;
            for (auto sit = states.begin(); sit != states.end(); ++sit) {
                const auto &map_entries = sit.value()["map"];
                for (auto mit = map_entries.begin(); mit != map_entries.end(); ++mit) {
                    std::string key = mit.key();
                    std::string val = mit.value();
                    if (val.empty())
                        shared_mapping.states[0].trie.insert_commit(key);
                    else
                        shared_mapping.states[0].trie.insert(key, val);
                    count++;
                }
            }
            ethio::logger.debug("ensure_mapping: loaded %d name entries", count);
        } catch (const std::exception &e) {
            ethio::logger.warning("Error loading names: %s", e.what());
        }
    } else {
        ethio::logger.debug("ensure_mapping: no names.json found");
    }
}

static bool has_connection(IBusEthiopicEngine *self)
{
    return ibus_service_get_connection(IBUS_SERVICE(self)) != nullptr;
}

static gboolean deferred_preedit_hide_cb(gpointer user_data)
{
    IBusEthiopicEngine *self = static_cast<IBusEthiopicEngine *>(user_data);
    IBusText *empty = ibus_text_new_from_static_string("");
    ibus_engine_update_preedit_text(IBUS_ENGINE(self), empty, 0, FALSE);
    g_object_unref(self);
    return G_SOURCE_REMOVE;
}

static void defer_preedit_hide(IBusEthiopicEngine *self)
{
    if (!has_connection(self)) return;
    g_idle_add(deferred_preedit_hide_cb, g_object_ref(self));
}

static bool is_word_boundary(const std::string &s)
{
    return s == " " || s == "\n" ||
           s == "፡" || s == "።" || s == "፣" || s == "፤" || s == "፥" || s == "፦";
}

static void commit(IBusEthiopicEngine *self);

static void hide_lookup(IBusEthiopicEngine *self)
{
    if (!self->priv->lookup_table) return;
    ibus_lookup_table_clear(self->priv->lookup_table);
    ibus_engine_update_lookup_table(IBUS_ENGINE(self),
            self->priv->lookup_table, FALSE);
    g_object_unref(self->priv->lookup_table);
    self->priv->lookup_table = nullptr;
}

static void show_suggestions(IBusEthiopicEngine *self)
{
    std::vector<std::string> results;

    if (!self->priv->word_buffer.empty()) {
        results = self->priv->wordlist.suggest(self->priv->word_buffer, 8);
    } else if (!self->priv->last_word.empty()) {
        results = self->priv->wordlist.suggest_next(self->priv->last_word, 8);
    }

    if (results.empty()) {
        results = self->priv->wordlist.top_words(8);
    }

    if (results.empty()) return;

    hide_lookup(self);

    self->priv->lookup_table = ibus_lookup_table_new(8, 0, TRUE, TRUE);
    g_object_ref_sink(self->priv->lookup_table);

    for (const auto &w : results) {
        IBusText *t = ibus_text_new_from_string(w.c_str());
        ibus_lookup_table_append_candidate(self->priv->lookup_table, t);
    }

    ibus_engine_update_lookup_table(IBUS_ENGINE(self),
            self->priv->lookup_table, TRUE);
}

static void accept_candidate(IBusEthiopicEngine *self)
{
    if (!self->priv->lookup_table) return;

    guint idx = ibus_lookup_table_get_cursor_pos(self->priv->lookup_table);
    IBusText *candidate = ibus_lookup_table_get_candidate(
            self->priv->lookup_table, idx);

    if (candidate) {
        std::string full(candidate->text);
        if (full.size() > self->priv->word_buffer.size() &&
            full.compare(0, self->priv->word_buffer.size(),
                         self->priv->word_buffer) == 0) {
            std::string suffix = full.substr(self->priv->word_buffer.size());
            suffix += " ";
            self->priv->core.append_produced(suffix);
            commit(self);
            self->priv->last_preedit.clear();
            defer_preedit_hide(self);
        }
    }

    hide_lookup(self);
}

static void commit(IBusEthiopicEngine *self)
{
    std::string text = self->priv->core.flush();
    if (text.empty()) return;

    ethio::logger.debug("commit: committing text='%s'", text.c_str());

    self->priv->last_commit = text;

    if (is_word_boundary(text)) {
        self->priv->last_word = self->priv->word_buffer;
        self->priv->word_buffer.clear();
    } else {
        self->priv->word_buffer += text;
    }

    if (!has_connection(self)) return;

    IBusText *ibus_text = ibus_text_new_from_static_string(text.c_str());
    ibus_engine_commit_text(IBUS_ENGINE(self), ibus_text);
}

static void preedit_update(IBusEthiopicEngine *self)
{
    std::string_view composing = self->priv->core.composing();

    ethio::logger.debug("preedit_update: composing='%s' (len=%zu)%s",
            composing.data(), composing.size(),
            composing.empty() ? " -> hiding" : "");

    self->priv->last_preedit = std::string(composing);

    if (!has_connection(self)) return;

    if (composing.empty()) {
        if (has_connection(self)) {
            IBusText *empty_text = ibus_text_new_from_static_string("");
            ibus_engine_update_preedit_text(IBUS_ENGINE(self), empty_text, 0, FALSE);
        }
        return;
    }

    size_t cursor_cp = self->priv->core.cursor();
    size_t cursor_byte = ethio::cp_offset_to_byte(composing, cursor_cp);
    ethio::logger.debug("preedit_update: cursor_cp=%zu cursor_byte=%zu "
            "text='%s' text_bytes=%zu",
            cursor_cp, cursor_byte, composing.data(), composing.size());
    IBusText *text = ibus_text_new_from_static_string(composing.data());
    text->attrs = ibus_attr_list_new();
    ibus_attr_list_append(text->attrs,
        ibus_attr_underline_new(IBUS_ATTR_UNDERLINE_SINGLE, 0,
                                static_cast<guint>(composing.size())));
    ibus_engine_update_preedit_text(IBUS_ENGINE(self), text,
                                    static_cast<guint>(cursor_byte),
                                    TRUE);
}

static gboolean
ibus_ethiopic_engine_process_key_event(IBusEngine *engine,
                                       guint keyval,
                                       guint keycode,
                                       guint state)
{
     ethio::logger.debug("process_key_event: is called.================");
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);

    if (state & IBUS_RELEASE_MASK) return FALSE;

    if (state & IBUS_MOD4_MASK) return FALSE;

    if (self->is_sensitive_field) return FALSE;

    if (state & IBUS_CONTROL_MASK) {
        if (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R) {
            self->priv->core.toggle_passthrough();
            self->priv->core.reset();
            commit(self);
            preedit_update(self);
            hide_lookup(self);
            return TRUE;
        }
        if (keyval == IBUS_KEY_space) {
            self->priv->core.finish_composition();
            commit(self);
            preedit_update(self);
            show_suggestions(self);
            return TRUE;
        }
        return FALSE;
    }

    if (self->priv->core.passthrough()) return FALSE;

    if (self->priv->lookup_table &&
        (keyval == IBUS_KEY_Tab || keyval == IBUS_ISO_Left_Tab)) {
        if (keyval == IBUS_ISO_Left_Tab) {
            guint pos = ibus_lookup_table_get_cursor_pos(
                    self->priv->lookup_table);
            if (pos == 0) {
                guint n = ibus_lookup_table_get_number_of_candidates(
                        self->priv->lookup_table);
                ibus_lookup_table_set_cursor_pos(self->priv->lookup_table, n - 1);
            } else {
                ibus_lookup_table_cursor_up(self->priv->lookup_table);
            }
        } else {
            guint n = ibus_lookup_table_get_number_of_candidates(
                    self->priv->lookup_table);
            guint pos = ibus_lookup_table_get_cursor_pos(
                    self->priv->lookup_table);
            if (pos + 1 >= n) {
                accept_candidate(self);
                return TRUE;
            }
            ibus_lookup_table_cursor_down(self->priv->lookup_table);
        }
        ibus_engine_update_lookup_table(IBUS_ENGINE(self),
                self->priv->lookup_table, TRUE);
        return TRUE;
    }

    if (self->priv->lookup_table) {
        if (keyval == IBUS_KEY_Return || keyval == IBUS_KEY_KP_Enter ||
            keyval == IBUS_KEY_space) {
            self->priv->core.finish_composition();
            commit(self);
            accept_candidate(self);
            return TRUE;
        }
        hide_lookup(self);
    }

    switch (keyval) {
    case IBUS_KEY_Escape:
        ethio::logger.debug("process_key_event: Escape -> reset");
        self->priv->core.reset();
        preedit_update(self);
        return FALSE;

    case IBUS_KEY_BackSpace: {
        std::string_view comp = self->priv->core.composing();
        if (!comp.empty()) {
            ethio::logger.debug("process_key_event: Backspace during "
                    "composition -> reset");
            self->priv->core.reset();
            preedit_update(self);
            return TRUE;
        }
        ethio::logger.debug("process_key_event: Backspace (nothing composing) "
                "-> pass through");
        return FALSE;
    }

    case IBUS_KEY_Return:
    case IBUS_KEY_KP_Enter:
    case IBUS_KEY_Left:
    case IBUS_KEY_KP_Left:
    case IBUS_KEY_Right:
    case IBUS_KEY_KP_Right:
    case IBUS_KEY_Home:
    case IBUS_KEY_KP_Home:
    case IBUS_KEY_End:
    case IBUS_KEY_KP_End:
    case IBUS_KEY_Up:
    case IBUS_KEY_KP_Up:
    case IBUS_KEY_Down:
    case IBUS_KEY_KP_Down:
    case IBUS_KEY_Page_Up:
    case IBUS_KEY_KP_Page_Up:
    case IBUS_KEY_Page_Down:
    case IBUS_KEY_KP_Page_Down:
    case IBUS_BUTTON1_MASK:
    case IBUS_BUTTON2_MASK:
    case IBUS_BUTTON3_MASK:
    case IBUS_BUTTON4_MASK:
    case IBUS_BUTTON5_MASK:

        if (self->priv->core.composing().empty())
            return FALSE;
        self->priv->core.finish_composition();
        commit(self);
        preedit_update(self);
        return FALSE;

    default:
        break;
    }

    gunichar uc = ibus_keyval_to_unicode(keyval);
    if (uc == 0) {
        ethio::logger.debug("process_key_event: keyval=0x%x -> uc=0, "
                "passing through", keyval);
        return FALSE;
    }

    char utf8[7] = {};
    gint len = g_unichar_to_utf8(uc, utf8);
    std::string key(utf8, static_cast<size_t>(len));

    bool handled = self->priv->core.filter(key);

    ethio::logger.debug("process_key_event: keyval=0x%x uc=U+%04X key='%s' "
            "filter=%s composing='%s' produced='%s'",
            keyval, uc, key.c_str(),
            handled ? "TRUE" : "FALSE",
            std::string(self->priv->core.composing()).c_str(),
            std::string(self->priv->core.produced_text()).c_str());

    if (!handled) {
        ethio::logger.debug("process_key_event: unmapped key='%s', "
                "appending to produced", key.c_str());
        self->priv->core.append_produced(key);
        handled = true;
    }

    commit(self);

    if (self->priv->word_buffer.empty() && !self->priv->last_word.empty()) {
        show_suggestions(self);
    }

    if (self->priv->core.composing().empty()) {
        self->priv->last_preedit.clear();
        defer_preedit_hide(self);
    } else {
        preedit_update(self);
    }

    return handled ? TRUE : FALSE;
}

static void
ibus_ethiopic_engine_set_content_type(IBusEngine *engine,
                                      guint purpose,
                                      guint hints)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    self->is_sensitive_field = (purpose == IBUS_INPUT_PURPOSE_PASSWORD ||
                                purpose == IBUS_INPUT_PURPOSE_PIN);
}

static void
ibus_ethiopic_engine_focus_out(IBusEngine *engine)
{
    ethio::logger.debug("focus_out: called");
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    hide_lookup(self);
    self->priv->word_buffer.clear();
    self->priv->last_word.clear();
    if (self->priv->core.passthrough()) {
        self->priv->core.toggle_passthrough();
    }
    self->priv->core.reset();
    preedit_update(self);
}

static void
ibus_ethiopic_engine_reset(IBusEngine *engine)
{
    ethio::logger.debug("reset: called");
    auto *self = IBUS_ETHIOPIC_ENGINE(engine);
    hide_lookup(self);
    self->priv->word_buffer.clear();
    self->priv->last_word.clear();
    self->priv->core.reset();
    preedit_update(self);
}

static void
ibus_ethiopic_engine_init(IBusEthiopicEngine *self)
{
    ethio::logger.debug("init: engine instance created");

    self->priv = new IBusEthiopicEnginePrivate();
    self->is_sensitive_field = false;

    ensure_mapping();
    if (!shared_mapping.states.empty()) {
        ethio::logger.debug("init: loading trie from state '%s'",
                shared_mapping.states[0].name.c_str());
        self->priv->core.load_trie(shared_mapping.states[0].trie);
    } else {
        ethio::logger.warning("init: no mapping states available, "
                "trie will be empty");
    }

    self->priv->wordlist = shared_wordlist;
}

static void
ibus_ethiopic_engine_finalize(GObject *obj)
{
    auto *self = IBUS_ETHIOPIC_ENGINE(obj);
    hide_lookup(self);
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

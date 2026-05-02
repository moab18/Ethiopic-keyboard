#include "engine.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static void feed_key(IBusEngine *engine, guint keyval)
{
    IBUS_ENGINE_GET_CLASS(engine)->process_key_event(engine, keyval, 0, 0);
}

static std::string last_commit(IBusEthiopicEngine *e)
{
    std::string s;
    s.swap(e->priv->last_commit);
    return s;
}

static std::string last_preedit(IBusEthiopicEngine *e)
{
    return e->priv->last_preedit;
}

#ifndef IBUS_ENGINE_NAME
#define IBUS_ENGINE_NAME "ethio:am"
#endif

int main()
{
    auto *engine = IBUS_ETHIOPIC_ENGINE(
        g_object_new(IBUS_TYPE_ETHIOPIC_ENGINE, "engine-name", IBUS_ENGINE_NAME, nullptr));

    auto *ibus_engine = IBUS_ENGINE(engine);
    std::cout << "Engine created, testing key input...\n";

    // Test 1: "he" → "ሀ"
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' → preedit 'ህ'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(last_preedit(engine) == "ሀ");
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'he' → preedit 'ሀ'\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሀ");
    std::cout << "  PASS: 'he' + space → commit 'ሀ'\n";

    // Test 2: "hee" → "ሄ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_e);
    assert(last_preedit(engine) == "ሀ");
    feed_key(ibus_engine, IBUS_KEY_e);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሄ");
    std::cout << "  PASS: 'hee' → commit 'ሄ'\n";

    // Test 3: "hW" → preedit "hW", no pending action
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_W);
    assert(last_preedit(engine) == "hW");
    std::cout << "  PASS: 'hW' → preedit 'hW'\n";

    feed_key(ibus_engine, IBUS_KEY_backslash);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'hW' + non-match → no commit (no pending action)\n";

    // Test 4: "'" delimiter
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_commit(engine) == "ህ");
    assert(last_preedit(engine) == "'");
    std::cout << "  PASS: 'h' + \"'\" → commit 'ህ', preedit \"'\"\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "ህ");
    std::cout << "  PASS: \"'\" + 'h' + space → commit 'ህ'\n";

    // Test 5: "''" → literal apostrophe
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_preedit(engine) == "'");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "'");
    std::cout << "  PASS: \"''\" → commit \"'\"\n";

    // Test 6: Backspace during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_BackSpace);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' + Backspace → reset (no commit)\n";

    // Test 7: Escape during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_Escape);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' + Escape → reset (no commit)\n";

    // Test 8: Release event (should be ignored)
    feed_key(ibus_engine, IBUS_KEY_h | IBUS_RELEASE_MASK);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: release event ignored\n";

    // Test 9: "h'h" → "ህህ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_commit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "ህ");
    std::cout << "  PASS: 'h' + \"'\" + 'h' + space → two 'ህ' commits\n";

    g_object_unref(engine);
    std::cout << "\nAll IBus engine tests passed.\n";
    return 0;
}

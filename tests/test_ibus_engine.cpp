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

    // Test 1: "h" -> preedit "ህ", "he" auto-commits "ሀ"
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' -> preedit 'ህ'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሀ");
    std::cout << "  PASS: 'he' -> auto-commit 'ሀ'\n";

    // Test 2: "hi" -> preedit "ሂ", "hie" auto-commits "ሄ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'hi' -> preedit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሄ");
    std::cout << "  PASS: 'hie' -> commit 'ሄ'\n";

    // Test 3: "hW" -> preedit "hW", then Left moves cursor (preedit stays)
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_W);
    assert(last_preedit(engine) == "hW");
    std::cout << "  PASS: 'hW' -> preedit 'hW'\n";

    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine) == "hW");
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'hW' + Left -> cursor moves (preedit stays)\n";

    // clean up: commit via space
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "hW ");
    assert(last_preedit(engine).empty());
    std::cout << "  PASS: 'hW' + Left + space -> commit 'hW '\n";

    // Test 4: "'" delimiter
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_commit(engine) == "ህ");
    assert(last_preedit(engine) == "'");
    std::cout << "  PASS: 'h' + \"'\" -> commit 'ህ', preedit \"'\"\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "ህ ");
    std::cout << "  PASS: \"'\" + 'h' + space -> commit 'ህ '\n";

    // Test 5: "''" -> literal apostrophe
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_preedit(engine) == "'");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "'");
    std::cout << "  PASS: \"''\" -> commit \"'\"\n";

    // Test 6: Backspace during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_BackSpace);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' + Backspace -> reset (no commit)\n";

    // Test 7: Escape during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_Escape);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'h' + Escape -> reset (no commit)\n";

    // Test 8: Release event (should be ignored)
    feed_key(ibus_engine, IBUS_KEY_h | IBUS_RELEASE_MASK);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: release event ignored\n";

    // Test 9: "h'h" -> "ህህ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(last_commit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "ህ ");
    std::cout << "  PASS: 'h' + \"'\" + 'h' + space -> commit 'ህ '\n";

    // Test 10: Arrow keys with preedit move cursor, preedit stays
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    assert(engine->priv->core.cursor() == 1);
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine) == "ሂ");
    assert(last_commit(engine).empty());
    assert(engine->priv->core.cursor() == 0);
    std::cout << "  PASS: 'hi' + Left -> cursor at 0 (preedit stays)\n";

    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(last_preedit(engine) == "ሂ");
    assert(engine->priv->core.cursor() == 1);
    std::cout << "  PASS: 'hi' + Left + Right -> cursor back at 1\n";

    feed_key(ibus_engine, IBUS_KEY_Home);
    assert(last_preedit(engine) == "ሂ");
    assert(last_commit(engine).empty());
    assert(engine->priv->core.cursor() == 0);
    std::cout << "  PASS: 'hi' + Home -> cursor at 0\n";

    feed_key(ibus_engine, IBUS_KEY_End);
    assert(last_preedit(engine) == "ሂ");
    assert(engine->priv->core.cursor() == 1);
    std::cout << "  PASS: 'hi' + End -> cursor at end\n";

    // Test 11: Unmapped printable ASCII committed as text
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_preedit(engine).empty());
    // space commits pending "ሂ" AND " " -> "ሂ " (pending from engine state)
    // Actually the previous state has "ሂ" in pending_text_ at "hi" node
    // Space triggers miss -> filter flushes "ሂ" to produced, then space is unmapped
    std::string s = last_commit(engine);
    assert(s.find("ሂ") != std::string::npos);
    std::cout << "  PASS: 'hi' + space commits pending\n";

    feed_key(ibus_engine, IBUS_KEY_numbersign);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "#");
    std::cout << "  PASS: unmapped '#' -> commit '#'\n";

    // Test 12: Pending composition + unmapped ASCII -> both committed
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_numbersign);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ#");
    std::cout << "  PASS: 'hi' + '#' -> commit 'ሂ#'\n";

    // Test 13: Arrow keys without preedit pass through
    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: Right with empty preedit -> pass through\n";

    // Test 14: Return/Enter commits pending
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Return);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Return -> commit 'ሂ'\n";

    // Test 14b: Cursor moves within single-codepoint preedit
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    assert(engine->priv->core.cursor() == 1);
    // Verify byte offset: cp_pos 1 in "ህ" (3 bytes) => byte 3
    assert(ethio::cp_offset_to_byte("ህ", 1) == 3);
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine) == "ህ");
    assert(engine->priv->core.cursor() == 0);
    // Verify byte offset: cp_pos 0 => byte 0
    assert(ethio::cp_offset_to_byte("ህ", 0) == 0);
    std::cout << "  PASS: 'h' + Left -> cursor cp 1->0 byte 3->0 (preedit stays)\n";

    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(last_preedit(engine) == "ህ");
    assert(engine->priv->core.cursor() == 1);
    assert(ethio::cp_offset_to_byte("ህ", 1) == 3);
    std::cout << "  PASS: 'h' + Left + Right -> cursor back cp=1 byte=3\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_commit(engine) == "ህ ");
    std::cout << "  PASS: 'h' + Left + Right + space -> commit 'ህ '\n";

    g_object_unref(engine);
    std::cout << "\nAll IBus engine tests passed.\n";
    return 0;
}

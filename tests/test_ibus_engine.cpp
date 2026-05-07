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

    // Test 3: Arrow keys during composition commit pending and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_W);
    assert(last_preedit(engine) == "hW");
    std::cout << "  PASS: 'hW' -> preedit 'hW'\n";

    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine).empty());
    std::cout << "  PASS: 'hW' + Left -> finish composition (reset), pass through\n";

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

    // Test 10: Arrow keys during preedit commit and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Left -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Right -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Home);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Home -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_End);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + End -> finish composition, commit 'ሂ'\n";

    // Test 11: Unmapped printable ASCII committed as text
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(last_preedit(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(last_preedit(engine).empty());
    std::string s = last_commit(engine);
    assert(s == "ሂ ");
    std::cout << "  PASS: 'hi' + space -> commit 'ሂ '\n";

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

    // Test 14b: Arrow keys during preedit commit and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(last_preedit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(last_preedit(engine).empty());
    assert(last_commit(engine) == "ህ");
    std::cout << "  PASS: 'h' + Left -> finish composition, commit 'ህ'\n";

    // Test 15: word_buffer accumulates committed syllables
    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_e);
    last_commit(engine);
    assert(engine->priv->word_buffer == "ሀ");
    std::cout << "  PASS: word_buffer accumulates 'ሀ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_o);
    last_commit(engine);
    assert(engine->priv->word_buffer == "ሀሆ");
    std::cout << "  PASS: word_buffer accumulates to 'ሀሆ'\n";

    // Test 16: word_buffer clears at word boundary, last_word saved
    feed_key(ibus_engine, IBUS_KEY_space);
    last_commit(engine);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "ሀሆ");
    std::cout << "  PASS: space clears word_buffer, last_word = 'ሀሆ'\n";

    // Test 17: last_word and word_buffer cleared on reset
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_e);
    last_commit(engine);
    assert(!engine->priv->word_buffer.empty());
    feed_key(ibus_engine, IBUS_KEY_space);
    last_commit(engine);
    assert(!engine->priv->last_word.empty());

    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    std::cout << "  PASS: reset clears word_buffer and last_word\n";

    // Test 18: word_buffer cleared but last_word empty after reset,
    // then space doesn't set last_word (empty word_buffer before space)
    feed_key(ibus_engine, IBUS_KEY_space);
    last_commit(engine);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    std::cout << "  PASS: space with empty word_buffer sets last_word to empty\n";

    g_object_unref(engine);
    std::cout << "\nAll IBus engine tests passed.\n";
    return 0;
}

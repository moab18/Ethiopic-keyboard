#include "engine.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include "ethio/logger.h"

static void feed_key(IBusEngine *engine, guint keyval)
{
    IBUS_ENGINE_GET_CLASS(engine)->process_key_event(engine, keyval, 0, 0);
}

static std::string drain_commit(IBusEthiopicEngine *e)
{
    std::string s;
    s.swap(e->priv->test_committed);
    return s;
}

static std::string drain_commit_all(IBusEthiopicEngine *e)
{
    std::string s;
    s.swap(e->priv->test_committed);
    return s;
}

static std::string preedit_text(IBusEthiopicEngine *e)
{
    return e->priv->test_preedit;
}

static bool preedit_visible(IBusEthiopicEngine *e)
{
    return e->priv->test_preedit_visible;
}

#ifndef IBUS_ENGINE_NAME
#define IBUS_ENGINE_NAME "ethio:am"
#endif

int main()
{
    ethio::logger.disable();
    
    auto *engine = IBUS_ETHIOPIC_ENGINE(
        g_object_new(IBUS_TYPE_ETHIOPIC_ENGINE, "engine-name", IBUS_ENGINE_NAME, nullptr)); 

    engine->priv->test_mode = true;
    
    auto *ibus_engine = IBUS_ENGINE(engine);
    std::cout << "Engine created (test mode), testing key input...\n";
   ethio::logger.enable("/tmp/ethio-test-" + std::string(g_get_user_name()) + ".log");
    // Test 1: "h" -> preedit "ህ", "he" auto-commits "ሀ"
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'h' -> preedit 'ህ'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሀ");
    std::cout << "  PASS: 'he' -> auto-commit 'ሀ'\n";

    // Test 2: "hi" -> preedit "ሂ", "hie" auto-commits "ሄ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'hi' -> preedit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሄ");
    std::cout << "  PASS: 'hie' -> commit 'ሄ'\n";

    // Test 3: Arrow keys during composition commit pending and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_W);
    assert(preedit_text(engine) == "hW");
    std::cout << "  PASS: 'hW' -> preedit 'hW'\n";

    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'hW' + Left -> finish composition (reset), pass through\n";

    // Test 4: "'" delimiter
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(drain_commit(engine) == "ህ");
    assert(preedit_text(engine) == "'");
    std::cout << "  PASS: 'h' + \"'\" -> commit 'ህ', preedit \"'\"\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(drain_commit(engine) == "ህ ");
    std::cout << "  PASS: \"'\" + 'h' + space -> commit 'ህ' then ' '\n";

    // Test 5: "''" -> literal apostrophe
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(preedit_text(engine) == "'");
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "'");
    std::cout << "  PASS: \"''\" -> commit \"'\"\n";

    // Test 6: Backspace during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_BackSpace);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'h' + Backspace -> reset (no commit)\n";

    // Test 7: Escape during composition
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_Escape);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'h' + Escape -> reset (no commit)\n";

    // Test 8: Release event (should be ignored)
    feed_key(ibus_engine, IBUS_KEY_h | IBUS_RELEASE_MASK);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: release event ignored\n";

    // Test 9: "h'h" -> "ህህ"
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_apostrophe);
    assert(drain_commit(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(drain_commit(engine) == "ህ ");
    std::cout << "  PASS: 'h' + \"'\" + 'h' + space -> commit 'ህ' then ' '\n";

    // Test 10: Arrow keys during preedit commit and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Left -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Right -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Home);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Home -> finish composition, commit 'ሂ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_End);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + End -> finish composition, commit 'ሂ'\n";

    // Test 11: Unmapped printable ASCII committed as text
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_space);
    assert(preedit_text(engine).empty());
    std::string s = drain_commit(engine);
    assert(s == "ሂ ");
    std::cout << "  PASS: 'hi' + space -> commit 'ሂ' then ' '\n";

    feed_key(ibus_engine, IBUS_KEY_numbersign);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "#");
    std::cout << "  PASS: unmapped '#' -> commit '#'\n";

    // Test 12: Pending composition + unmapped ASCII -> both committed
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_numbersign);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ#");
    std::cout << "  PASS: 'hi' + '#' -> commit 'ሂ' then '#'\n";

    // Test 13: Arrow keys without preedit pass through
    feed_key(ibus_engine, IBUS_KEY_Right);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: Right with empty preedit -> pass through\n";

    // Test 14: Return/Enter commits pending
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_i);
    assert(preedit_text(engine) == "ሂ");
    feed_key(ibus_engine, IBUS_KEY_Return);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ሂ");
    std::cout << "  PASS: 'hi' + Return -> commit 'ሂ'\n";

    // Test 14b: Arrow keys during preedit commit and pass through
    feed_key(ibus_engine, IBUS_KEY_h);
    assert(preedit_text(engine) == "ህ");
    feed_key(ibus_engine, IBUS_KEY_Left);
    assert(preedit_text(engine).empty());
    assert(drain_commit(engine) == "ህ");
    std::cout << "  PASS: 'h' + Left -> finish composition, commit 'ህ'\n";

    // Test 15: word_buffer accumulates committed syllables
    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_e);
    drain_commit(engine);
    assert(engine->priv->word_buffer == "ሀ");
    std::cout << "  PASS: word_buffer accumulates 'ሀ'\n";

    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_o);
    drain_commit(engine);
    assert(engine->priv->word_buffer == "ሀሆ");
    std::cout << "  PASS: word_buffer accumulates to 'ሀሆ'\n";

    // Test 16: word_buffer clears at word boundary, last_word saved
    feed_key(ibus_engine, IBUS_KEY_space);
    drain_commit(engine);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "ሀሆ");
    std::cout << "  PASS: space clears word_buffer, last_word = 'ሀሆ'\n";

    // Test 17: last_word and word_buffer cleared on reset
    feed_key(ibus_engine, IBUS_KEY_h);
    feed_key(ibus_engine, IBUS_KEY_e);
    drain_commit(engine);
    assert(!engine->priv->word_buffer.empty());
    feed_key(ibus_engine, IBUS_KEY_space);
    drain_commit(engine);
    assert(!engine->priv->last_word.empty());

    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    std::cout << "  PASS: reset clears word_buffer and last_word\n";

    // Test 18: word_buffer cleared but last_word empty after reset,
    // then space doesn't set last_word (empty word_buffer before space)
    feed_key(ibus_engine, IBUS_KEY_space);
    drain_commit(engine);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word.empty());
    std::cout << "  PASS: space with empty word_buffer sets last_word to empty\n";

    // ══════════ Test 19: multi-word phrase "ato lema " (አቶ ለማ) ══════
    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));

    feed_key(ibus_engine, IBUS_KEY_a);
    assert(engine->priv->last_preedit == "አ");
    std::cout << "  PASS: 'a' -> preedit 'አ'\n";

    feed_key(ibus_engine, IBUS_KEY_t);
    assert(engine->priv->last_preedit == "ት");
    std::string c1 = drain_commit(engine);
    assert(c1 == "አ");
    assert(engine->priv->word_buffer == "አ");
    std::cout << "  PASS: 'at' -> commit 'አ', preedit 'ት', word_buffer='አ'\n";

    feed_key(ibus_engine, IBUS_KEY_o);
    assert(engine->priv->last_preedit.empty());
    std::string c2 = drain_commit(engine);
    assert(c2 == "ቶ");
    assert(engine->priv->word_buffer == "አቶ");
    std::cout << "  PASS: 'ato' -> commit 'ቶ', preedit empty, word_buffer='አቶ'\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "አቶ");
    drain_commit(engine);
    std::cout << "  PASS: space after 'ato' -> boundary, last_word='አቶ'\n";

    feed_key(ibus_engine, IBUS_KEY_l);
    assert(engine->priv->last_preedit == "ል");
    std::cout << "  PASS: 'l' -> preedit 'ል'\n";

    feed_key(ibus_engine, IBUS_KEY_e);
    assert(engine->priv->last_preedit.empty());
    std::string c3 = drain_commit(engine);
    assert(c3 == "ለ");
    assert(engine->priv->word_buffer == "ለ");
    std::cout << "  PASS: 'le' -> commit 'ለ', word_buffer='ለ'\n";

    feed_key(ibus_engine, IBUS_KEY_m);
    assert(engine->priv->last_preedit == "ም");
    assert(engine->priv->word_buffer == "ለ");
    std::cout << "  PASS: 'm' -> preedit 'ም', word_buffer still 'ለ'\n";

    feed_key(ibus_engine, IBUS_KEY_a);
    assert(engine->priv->last_preedit == "ማ");
    assert(engine->priv->word_buffer == "ለ");
    std::cout << "  PASS: 'ma' -> preedit 'ማ', word_buffer still 'ለ'\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    assert(engine->priv->last_preedit.empty());
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "ለማ");
    std::string c4 = drain_commit(engine);
    assert(c4 == "ማ ");
    std::cout << "  PASS: space after 'lema' -> commit preedit, boundary, last_word='ለማ'\n";

    // ══════ Test 20: "ato lema" without trailing space — preedit stays pending ══════
    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));

    feed_key(ibus_engine, IBUS_KEY_a);
    feed_key(ibus_engine, IBUS_KEY_t);
    feed_key(ibus_engine, IBUS_KEY_o);
    drain_commit(engine);
    assert(engine->priv->word_buffer == "አቶ");
    assert(engine->priv->last_preedit.empty());
    std::cout << "  PASS: 'ato' -> word_buffer='አቶ'\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    drain_commit(engine);
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "አቶ");
    std::cout << "  PASS: space -> boundary, last_word='አቶ'\n";

    feed_key(ibus_engine, IBUS_KEY_l);
    feed_key(ibus_engine, IBUS_KEY_e);
    drain_commit(engine);
    assert(engine->priv->word_buffer == "ለ");
    std::cout << "  PASS: 'le' -> word_buffer='ለ'\n";

    feed_key(ibus_engine, IBUS_KEY_m);
    feed_key(ibus_engine, IBUS_KEY_a);
    assert(engine->priv->test_preedit == "ማ");
    assert(engine->priv->test_preedit_visible);
    assert(engine->priv->word_buffer == "ለ");
    assert(drain_commit(engine).empty());
    std::cout << "  PASS: 'ma' -> preedit 'ማ', word_buffer still 'ለ', no commit yet\n";

    // ══════ Test 21: single-word with branch preedit "selam " (ሰላም) ══════
    IBUS_ENGINE_GET_CLASS(engine)->reset(IBUS_ENGINE(engine));

    feed_key(ibus_engine, IBUS_KEY_s);
    feed_key(ibus_engine, IBUS_KEY_e);
    assert(engine->priv->word_buffer == "ሰ");
    assert(engine->priv->last_preedit.empty());
    std::cout << "  PASS: 'se' -> commit 'ሰ', word_buffer='ሰ'\n";

    feed_key(ibus_engine, IBUS_KEY_l);
    assert(engine->priv->last_preedit == "ል");
    std::cout << "  PASS: 'l' -> preedit 'ል'\n";

    feed_key(ibus_engine, IBUS_KEY_a);
    assert(engine->priv->last_preedit == "ላ");
    assert(engine->priv->word_buffer == "ሰ");
    std::cout << "  PASS: 'la' -> preedit 'ላ', word_buffer still 'ሰ'\n";

    feed_key(ibus_engine, IBUS_KEY_m);
    assert(engine->priv->last_preedit == "ም");
    assert(engine->priv->word_buffer == "ሰላ");
    std::cout << "  PASS: 'm' -> flush 'ላ', commit, preedit 'ም', word_buffer='ሰላ'\n";

    feed_key(ibus_engine, IBUS_KEY_space);
    assert(engine->priv->last_preedit.empty());
    assert(engine->priv->word_buffer.empty());
    assert(engine->priv->last_word == "ሰላም");
    std::cout << "  PASS: space after 'selam' -> commit 'ም', boundary, last_word='ሰላም'\n";

    g_object_unref(engine);
    std::cout << "\nAll IBus engine tests passed.\n";
    return 0;
}

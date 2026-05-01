#include "ethio/engine.h"
#include "ethio/mapping.h"

#include <cassert>
#include <iostream>
#include <string>

static std::string data_path()
{
#ifdef DATA_DIR
    return DATA_DIR "/amharic/am-sera.json";
#else
    return "data/amharic/am-sera.json";
#endif
}

int main()
{
    auto mf = ethio::load_mapping_file(data_path());
    assert(!mf.states.empty());
    auto &trie = mf.states[0].trie;

    ethio::Engine eng;
    eng.load_trie(trie);

    // --- Test 1: Single key shows pending in preedit ---
    {
        bool handled = eng.filter("h");
        assert(handled);
        assert(eng.composing() == "ህ");       // pending "h" action
        assert(eng.flush().empty());           // nothing committed yet
        std::cout << "  PASS: 'h' -> pending 'ህ'\n";
        eng.reset();
    }

    // --- Test 2: "he" shows "ሀ" pending, commits on mismatch ---
    {
        eng.filter("h");
        eng.filter("e");
        assert(eng.composing() == "ሀ");       // pending "he" action
        assert(eng.flush().empty());

        eng.filter(" ");                       // space triggers branch-miss
        assert(eng.flush() == "ሀ");
        assert(eng.composing().empty());
        std::cout << "  PASS: 'he' + space -> flush 'ሀ'\n";
        eng.reset();
    }

    // --- Test 3: "hee" replaces "he" -> auto-commits "ሄ" ---
    {
        eng.filter("h");
        eng.filter("e");
        assert(eng.composing() == "ሀ");       // pending "he"
        eng.filter("e");
        // "hee" node has no children → auto-committed
        assert(eng.composing().empty());
        assert(eng.flush() == "ሄ");
        std::cout << "  PASS: 'hee' -> flush 'ሄ'\n";
        eng.reset();
    }

    // --- Test 4: Labiovelar "hWa" -> "ኋ" ---
    {
        eng.filter("h");
        eng.filter("W");
        assert(eng.composing() == "hW");       // no pending at "hW"
        eng.filter("a");
        assert(eng.flush() == "ኋ");           // leaf, auto-committed
        std::cout << "  PASS: 'hWa' -> flush 'ኋ'\n";
        eng.reset();
    }

    // --- Test 5: Punctuation ':' composes, '::' replaces ---
    {
        eng.filter(":");
        assert(eng.composing() == "፡");       // pending ":"
        eng.filter(":");
        assert(eng.composing() == "።");       // pending "::"
        // "::" has child ":" for ":::" → not a leaf, stays pending
        eng.filter(" ");                       // miss
        assert(eng.flush() == "።");
        std::cout << "  PASS: '::' + space -> flush '።'\n";
        eng.reset();
    }

    // --- Test 6: ":::" -> literal ":" ---
    {
        eng.filter(":");
        eng.filter(":");
        eng.filter(":");
        assert(eng.flush() == ":");
        std::cout << "  PASS: ':::' -> flush ':'\n";
        eng.reset();
    }

    // --- Test 7: Unhandled key from root ---
    {
        bool handled = eng.filter("#");
        assert(!handled);
        assert(eng.composing().empty());
        assert(eng.flush().empty());
        std::cout << "  PASS: '#' not handled\n";
        eng.reset();
    }

    // --- Test 8: "h" then "#" (truly unhandled) commits "ህ" ---
    {
        eng.filter("h");
        bool handled = eng.filter("#");
        assert(!handled);
        assert(eng.flush() == "ህ");
        assert(eng.composing().empty());
        std::cout << "  PASS: 'h' + '#' -> flush 'ህ', '#' not handled\n";
        eng.reset();
    }

    // --- Test 9: Multiple syllables accumulate ---
    {
        eng.filter("h");
        eng.filter("e");
        eng.filter(" ");
        eng.filter("l");
        eng.filter("e");
        eng.filter(" ");
        eng.filter("b");
        eng.filter("e");
        eng.filter(" ");
        assert(eng.flush() == "ሀለበ");
        std::cout << "  PASS: sequence 'he le be' -> 'ሀለበ'\n";
        eng.reset();
    }

    // --- Test 10: Commit delimiter "'" escapes to literal "''" ---
    {
        eng.filter("'");
        assert(eng.composing() == "'");        // no pending text, just raw key
        assert(eng.flush().empty());

        bool handled = eng.filter("'");
        assert(handled);
        assert(eng.composing().empty());
        assert(eng.flush() == "'");
        std::cout << "  PASS: \"''\" -> flush literal \"'\"\n";
        eng.reset();
    }

    // --- Test 11: "'" commit flushes pending ---
    {
        eng.filter("h");
        assert(eng.composing() == "ህ");

        eng.filter("'");                       // miss at 'h' → flush "ህ", retry '
        assert(eng.flush() == "ህ");
        assert(eng.composing() == "'");        // pending nothing, raw key
        std::cout << "  PASS: 'h' + \"'\" -> flush 'ህ'\n";
        eng.reset();
    }

    // --- Test 12: "'" miss does nothing ---
    {
        eng.filter("'");
        assert(eng.composing() == "'");
        eng.filter("#");                       // miss at ' → no pending → nothing
        assert(eng.flush().empty());
        assert(eng.composing().empty());
        std::cout << "  PASS: \"'\" + miss -> no output\n";
        eng.reset();
    }

    // --- Test 13: Slash prefix "/he" -> "ኀ" ---
    {
        eng.filter("/");
        eng.filter("h");
        eng.filter("e");
        assert(eng.composing() == "ኀ");       // pending
        eng.filter(" ");                       // commit trigger
        assert(eng.flush() == "ኀ");
        std::cout << "  PASS: '/he' + space -> flush 'ኀ'\n";
        eng.reset();
    }

    // --- Test 14: Self-escape "//" -> "/" ---
    {
        eng.filter("/");
        assert(eng.composing() == "/");        // raw key, no pending
        eng.filter("/");
        assert(eng.composing().empty());
        assert(eng.flush() == "/");
        std::cout << "  PASS: '//' -> '/'\n";
        eng.reset();
    }

    // --- Test 15: 5th order "be" vs "bE" ---
    {
        eng.filter("b"); eng.filter("e");
        eng.filter(" ");
        assert(eng.flush() == "በ");

        eng.filter("b"); eng.filter("E");
        assert(eng.flush() == "ቤ");           // leaf, auto-commit
        std::cout << "  PASS: 'be'='በ', 'bE'='ቤ'\n";
        eng.reset();
    }

    // --- Test 16: Composing returns raw keys when no pending action ---
    {
        eng.filter("h");
        eng.filter("W");
        assert(eng.composing() == "hW");       // "hW" has no action
        eng.filter("a");
        assert(eng.flush() == "ኋ");
        std::cout << "  PASS: 'hW' shown as raw keys in preedit\n";
        eng.reset();
    }

    // --- Test 17: Accumulated flush over multiple segments ---
    {
        eng.filter("h"); eng.filter("e"); eng.filter(" ");
        eng.filter("l"); eng.filter("e"); eng.filter(" ");
        std::string all = eng.flush();
        assert(all == "ሀለ");
        std::cout << "  PASS: accumulated flush = 'ሀለ'\n";
        eng.reset();
    }

    std::cout << "\nAll engine tests passed.\n";
    return 0;
}

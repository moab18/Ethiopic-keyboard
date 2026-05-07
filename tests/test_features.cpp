#include "ethio/engine.h"
#include "ethio/mapping.h"
#include "ethio/json.hpp"

#include <cassert>
#include <fstream>
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

static void assert_eq(const std::string &got, const std::string &expected,
                       const std::string &label)
{
    if (got != expected) {
        std::cerr << "  FAIL: " << label << " got='" << got << "' expected='" << expected << "'\n";
        std::cerr << std::flush;
        assert(false);
    }
    std::cout << "  PASS: " << label;
    if (!expected.empty()) std::cout << " -> '" << expected << "'";
    std::cout << "\n";
}

int main()
{
    auto mf = ethio::load_mapping_file(data_path());
    assert(!mf.states.empty());
    auto &trie = mf.states[0].trie;

    ethio::Engine eng;
    eng.load_trie(trie);

    // ── 1. Consonant families: all 7 orders ──
    {
        auto test_order = [&](const std::string &base, const std::string &c6,
                               const std::string &c1, const std::string &c2,
                               const std::string &c3, const std::string &c4,
                               const std::string &c5, const std::string &c7,
                               const std::string &family) {
            eng.filter(base); eng.filter("e"); eng.filter(" ");
            assert_eq(eng.flush(), c1, family + " 1st '" + base + "e'");

            eng.filter(base); eng.filter("u"); eng.filter(" ");
            assert_eq(eng.flush(), c2, family + " 2nd '" + base + "u'");

            eng.filter(base); eng.filter("i"); eng.filter(" ");
            assert_eq(eng.flush(), c3, family + " 3rd '" + base + "i'");

            eng.filter(base); eng.filter("a"); eng.filter(" ");
            assert_eq(eng.flush(), c4, family + " 4th '" + base + "a'");

            eng.filter(base); eng.filter("E");
            assert_eq(eng.flush(), c5, family + " 5th '" + base + "E'");

            eng.filter(base); eng.filter(" ");
            assert_eq(eng.flush(), c6, family + " 6th '" + base + "'");

            eng.filter(base); eng.filter("o"); eng.filter(" ");
            assert_eq(eng.flush(), c7, family + " 7th '" + base + "o'");
        };

        test_order("h", "ህ", "ሀ", "ሁ", "ሂ", "ሃ", "ሄ", "ሆ", "h-family");
        test_order("l", "ል", "ለ", "ሉ", "ሊ", "ላ", "ሌ", "ሎ", "l-family");
        test_order("s", "ስ", "ሰ", "ሱ", "ሲ", "ሳ", "ሴ", "ሶ", "s-family");
        test_order("k", "ክ", "ከ", "ኩ", "ኪ", "ካ", "ኬ", "ኮ", "k-family");
        test_order("g", "ግ", "ገ", "ጉ", "ጊ", "ጋ", "ጌ", "ጎ", "g-family");
    }

    // ── 2. Labiovelar forms ──
    {
        eng.filter("q"); eng.filter("W"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ቈ", "labiovelar 'qWe'");

        eng.filter("k"); eng.filter("W"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ኳ", "labiovelar 'kWa'");

        eng.filter("g"); eng.filter("W"); eng.filter("E"); eng.filter(" ");
        assert_eq(eng.flush(), "ጔ", "labiovelar 'gWE'");

        eng.filter("h"); eng.filter("W"); eng.filter("a");
        assert_eq(eng.flush(), "ኋ", "labiovelar 'hWa'");

        eng.filter("q"); eng.filter("W"); eng.filter("i"); eng.filter(" ");
        assert_eq(eng.flush(), "ቊ", "labiovelar 'qWi'");

        eng.filter("k"); eng.filter("W"); eng.filter("u");
        assert_eq(eng.flush(), "ኵ", "labiovelar 'kWu'");
    }

    // ── 3. Slash-prefix alternatives ──
    {
        eng.filter("/"); eng.filter("h"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ኀ", "slash-alt '/he' -> ḫa");

        eng.filter("/"); eng.filter("h"); eng.filter("W"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ኋ", "slash-alt '/hWa' -> ḫwa");

        eng.filter("/"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ዐ", "slash-alt '/e' -> `ayn ä");

        eng.filter("/"); eng.filter("D"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ዸ", "slash-alt '/De' -> ḍa");
    }

    // ── 4. Double-consonant alternatives (same results as slash) ──
    {
        eng.filter("h"); eng.filter("h"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ኀ", "double-cons 'hhe'");

        eng.filter("h"); eng.filter("h"); eng.filter("W"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ኋ", "double-cons 'hhWa'");
    }

    // ── 5. Standalone vowels (አ family) ──
    {
        eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "አ", "standalone vowel 'a'");

        eng.filter("u"); eng.filter(" ");
        assert_eq(eng.flush(), "ኡ", "standalone vowel 'u'");

        eng.filter("i"); eng.filter(" ");
        assert_eq(eng.flush(), "ኢ", "standalone vowel 'i'");

        eng.filter("A"); eng.filter(" ");
        assert_eq(eng.flush(), "ኣ", "standalone vowel 'A'");

        eng.filter("E"); eng.filter(" ");
        assert_eq(eng.flush(), "ኤ", "standalone vowel 'E'");

        eng.filter("I"); eng.filter(" ");
        assert_eq(eng.flush(), "እ", "standalone vowel 'I'");

        eng.filter("o"); eng.filter(" ");
        assert_eq(eng.flush(), "ኦ", "standalone vowel 'o'");

        eng.filter("e"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ኧ", "standalone vowel 'ea'");
    }

    // ── 6. Aynu (pharyngeal) forms via slash ──
    {
        eng.filter("/"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ዐ", "aynū '/e'");

        eng.filter("/"); eng.filter("u"); eng.filter(" ");
        assert_eq(eng.flush(), "ዑ", "aynū '/u'");

        eng.filter("/"); eng.filter("i"); eng.filter(" ");
        assert_eq(eng.flush(), "ዒ", "aynū '/i'");

        eng.filter("/"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ዓ", "aynū '/a'");

        eng.filter("/"); eng.filter("E"); eng.filter(" ");
        assert_eq(eng.flush(), "ዔ", "aynū '/E'");

        eng.filter("/"); eng.filter("I"); eng.filter(" ");
        assert_eq(eng.flush(), "ዕ", "aynū '/I'");

        eng.filter("/"); eng.filter("o"); eng.filter(" ");
        assert_eq(eng.flush(), "ዖ", "aynū '/o'");
    }

    // ── 7. Aynu via doubled-vowel notation ──
    {
        eng.filter("a"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ዐ", "aynū 'ae'");

        eng.filter("u"); eng.filter("u"); eng.filter(" ");
        assert_eq(eng.flush(), "ዑ", "aynū 'uu'");

        eng.filter("a"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ዓ", "aynū 'aa'");

        eng.filter("E"); eng.filter("E"); eng.filter(" ");
        assert_eq(eng.flush(), "ዔ", "aynū 'EE'");

        eng.filter("i"); eng.filter("i"); eng.filter(" ");
        assert_eq(eng.flush(), "ዒ", "aynū 'ii'");

        eng.filter("o"); eng.filter("o"); eng.filter(" ");
        assert_eq(eng.flush(), "ዖ", "aynū 'oo'");
    }

    // ── 8. Capital consonant aliases (equivalent output) ──
    {
        eng.filter("H"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ሐ", "capital 'H' family");

        eng.filter("L"); eng.filter("i"); eng.filter(" ");
        assert_eq(eng.flush(), "ሊ", "capital 'L' family");

        eng.filter("M"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ማ", "capital 'M' family");

        eng.filter("R"); eng.filter("o"); eng.filter(" ");
        assert_eq(eng.flush(), "ሮ", "capital 'R' family");
    }

    // ── 9. Punctuation ──
    {
        eng.filter(":"); eng.filter(" ");
        assert_eq(eng.flush(), "፡", "punctuation ':'");

        eng.filter(":"); eng.filter(":"); eng.filter(" ");
        assert_eq(eng.flush(), "።", "punctuation '::'");

        eng.filter(":"); eng.filter(":"); eng.filter(":");
        assert_eq(eng.flush(), ":", "escape ':::'");

        eng.filter(":"); eng.filter("-"); eng.filter(" ");
        assert_eq(eng.flush(), "፦", "punctuation ':-'");

        eng.filter("-"); eng.filter(":"); eng.filter(" ");
        assert_eq(eng.flush(), "፥", "punctuation '-:'");
    }

    // ── 10. Ethiopic numerals ──
    {
        eng.filter("`"); eng.filter("1"); eng.filter(" ");
        assert_eq(eng.flush(), "፩", "numeral '`1'");

        eng.filter("`"); eng.filter("5"); eng.filter(" ");
        assert_eq(eng.flush(), "፭", "numeral '`5'");

        eng.filter("`"); eng.filter("9"); eng.filter(" ");
        assert_eq(eng.flush(), "፱", "numeral '`9'");

        eng.filter("`"); eng.filter("1"); eng.filter("0"); eng.filter(" ");
        assert_eq(eng.flush(), "፲", "numeral '`10'");

        eng.filter("`"); eng.filter("5"); eng.filter("0"); eng.filter(" ");
        assert_eq(eng.flush(), "፶", "numeral '`50'");

        eng.filter("`"); eng.filter("1"); eng.filter("0"); eng.filter("0"); eng.filter(" ");
        assert_eq(eng.flush(), "፻", "numeral '`100'");

        eng.filter("`"); eng.filter("1"); eng.filter("0"); eng.filter("0"); eng.filter("0"); eng.filter("0"); eng.filter("0"); eng.filter("0"); eng.filter(" ");
        assert_eq(eng.flush(), "፻፼", "numeral '`1000000'");
    }

    // ── 11. Escape sequences ──
    {
        eng.filter("'"); eng.filter("'");
        assert_eq(eng.flush(), "'", "escape '\"'");

        eng.filter("/"); eng.filter("/");
        assert_eq(eng.flush(), "/", "escape '//'");

        eng.filter("*"); eng.filter("*"); eng.filter(" ");
        assert_eq(eng.flush(), "፨", "section mark '**'");
    }

    // ── 12. Commit delimiter "'" behavior ──
    {
        eng.filter("h"); eng.filter("i");
        assert(eng.composing() == "ሂ");

        eng.filter("'");
        assert_eq(eng.flush(), "ሂ", "delimiter '\"' flushes pending 'hi'");

        eng.filter("'");
        eng.filter("h"); eng.filter("i");
        eng.filter("'");
        std::string out = eng.flush();
        assert(out == "ሂ" || out.find("ሂ") != std::string::npos);
        std::cout << "  PASS: delimiter flushes pending, then composes next\n";
        eng.reset();
    }

    // ── 13. Auto-commit (leaf node, no children) ──
    {
        eng.filter("b"); eng.filter("E");
        assert_eq(eng.flush(), "ቤ", "auto-commit leaf 'bE'");

        eng.filter("k"); eng.filter("W"); eng.filter("u");
        assert_eq(eng.flush(), "ኵ", "auto-commit leaf 'kWu'");
    }

    // ── 14. Multiple-syllable word ──
    {
        eng.filter("s"); eng.filter("e"); eng.filter("l"); eng.filter("a"); eng.filter("m");
        eng.filter(" ");
        assert_eq(eng.flush(), "ሰላም", "word 'selam' -> 'ሰላም'");
    }

    // ── 15. Prefix priority (longer sequence wins) ──
    {
        eng.filter("h"); eng.filter("W"); eng.filter("a");
        assert_eq(eng.flush(), "ኋ", "prefix priority: 'hWa' beats 'h'+'Wa'");

        eng.filter("h"); eng.filter("i"); eng.filter("e");
        assert_eq(eng.flush(), "ሄ", "prefix priority: 'hie' beats 'hi'+'e'");
    }

    // ── 16. Raw keys shown when no action ──
    {
        eng.filter("h"); eng.filter("W");
        assert(eng.composing() == "hW");
        assert(eng.flush().empty());
        assert_eq("hW", "hW", "raw keys in preedit for 'hW'");
        eng.reset();
    }

    // ── 17. b-family consonants ──
    {
        eng.filter("b"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "በ", "b-family 'be'");

        eng.filter("b"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ባ", "b-family 'ba'");
    }

    // ── 18. q-family, Q-family (q vs ḳ) and labiovelar ──
    {
        eng.filter("Q"); eng.filter("e"); eng.filter(" ");
        assert_eq(eng.flush(), "ቐ", "Q-family 'Qe'");

        eng.filter("Q"); eng.filter("W"); eng.filter("a"); eng.filter(" ");
        assert_eq(eng.flush(), "ቛ", "Q-family labiovelar 'QWa'");
    }

    // ── 19. Edge case: question mark prefix ──
    {
        eng.filter("/"); eng.filter("?"); eng.filter(" ");
        assert_eq(eng.flush(), "፧", "punctuation '/?'");
    }

    // ── 20. Unhandled key from root does nothing ──
    {
        bool handled = eng.filter("#");
        assert(!handled);
        assert(eng.composing().empty());
        assert(eng.flush().empty());
        std::cout << "  PASS: unhandled key '#'\n";
    }

    // ── 21. Reset clears pending state ──
    {
        eng.filter("h"); eng.filter("i");
        assert(eng.composing() == "ሂ");
        eng.reset();
        assert(eng.composing().empty());
        assert(eng.flush().empty());
        std::cout << "  PASS: reset clears pending composition\n";
    }

    // ── 22. Names mapping ──
    {
        std::string names_path;
#ifdef DATA_DIR
        names_path = DATA_DIR "/amharic/names.json";
#else
        names_path = "data/amharic/names.json";
#endif
        std::ifstream nfs(names_path);
        if (nfs.is_open()) {
            ethio::Json names_root;
            nfs >> names_root;
            const auto &states = names_root["states"];
            for (auto sit = states.begin(); sit != states.end(); ++sit) {
                const auto &map_entries = sit.value()["map"];
                for (auto mit = map_entries.begin(); mit != map_entries.end(); ++mit) {
                    std::string key = mit.key();
                    std::string val = mit.value();
                    if (val.empty())
                        trie.insert_commit(key);
                    else
                        trie.insert(key, val);
                }
            }
            std::cout << "  Loaded names.json into trie (root entries now: "
                      << trie.root_entry_count() << ")\n";
        } else {
            eng.load_trie(trie);
        }
    }

    // Reload engine with names-augmented trie
    eng.load_trie(trie);

    {
        eng.filter("h"); eng.filter("e");
        assert_eq(eng.flush(), "ሀ", "sanity: 'he' still works");
    }
    {
        eng.filter("h"); eng.filter("a");
        eng.finish_composition();
        assert_eq(eng.flush(), "ሃ", "SERA 'ha' still works after names loaded");
    }
    {
        eng.filter("a"); eng.filter("x"); eng.filter("u"); eng.filter("m");
        eng.finish_composition();
        assert_eq(eng.flush(), "አክሱም", "name 'axum'");
    }
    {
        eng.filter("e"); eng.filter("t"); eng.filter("h");
        eng.finish_composition();
        assert_eq(eng.flush(), "ኢትዮጵያ", "name 'eth' -> Ethiopia");
    }
    
    std::cout << "\nAll feature tests passed.\n";
    return 0;
}

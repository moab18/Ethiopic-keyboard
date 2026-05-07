#include "ethio/wordlist.h"

#include <cassert>
#include <iostream>
#include <string>

static std::string data_path()
{
#ifdef DATA_DIR
    return DATA_DIR "/amharic/wordlist.json";
#else
    return "data/amharic/wordlist.json";
#endif
}

static std::string bigrams_path()
{
#ifdef DATA_DIR
    return DATA_DIR "/amharic/bigrams.json";
#else
    return "data/amharic/bigrams.json";
#endif
}

int main()
{
    ethio::WordList wl;
    wl.load(data_path());

    std::cout << "Loaded " << wl.size() << " unique words in "
              << (wl.has_tag("common")  ? "common "  : "")
              << (wl.has_tag("places")  ? "places "  : "")
              << (wl.has_tag("people")  ? "people "  : "")
              << (wl.has_tag("organizations") ? "organizations" : "")
              << "\n";

    assert(wl.size() > 0);
    assert(wl.has_tag("common"));
    assert(wl.has_tag("places"));
    assert(wl.has_tag("people"));
    assert(wl.has_tag("organizations"));
    assert(!wl.has_tag("nonexistent"));

    // ── prefix suggest: "አ" ──
    {
        auto results = wl.suggest("አ", 10);
        assert(!results.empty());
        for (const auto &w : results)
            assert(w.size() >= 2 && w.rfind("አ", 0) == 0);
        std::cout << "  PASS: prefix 'አ' -> " << results.size() << " suggestions\n";
    }

    // ── prefix suggest: "ሰላ" ──
    {
        auto results = wl.suggest("ሰላ", 5);
        assert(!results.empty());
        assert(results[0] == "ሰላም");
        std::cout << "  PASS: prefix 'ሰላ' -> ሰላም\n";
    }

    // ── prefix suggest: exact match ──
    {
        auto results = wl.suggest("ሰላም", 5);
        assert(!results.empty());
        assert(results[0] == "ሰላም");
        std::cout << "  PASS: prefix 'ሰላም' -> exact match\n";
    }

    // ── prefix suggest: no match ──
    {
        auto results = wl.suggest("ዘብርቀ", 5);
        assert(results.empty());
        std::cout << "  PASS: prefix 'ዘብርቀ' -> no match\n";
    }

    // ── prefix suggest: empty prefix ──
    {
        auto results = wl.suggest("", 5);
        assert(results.empty());
        std::cout << "  PASS: empty prefix -> no results\n";
    }

    // ── prefix suggest: max_results limits ──
    {
        auto results = wl.suggest("አ", 3);
        assert(results.size() == 3);
        std::cout << "  PASS: max_results=3 limits to 3\n";
    }

    // ── prefix suggest: max_results=0 ──
    {
        auto results = wl.suggest("አ", 0);
        assert(results.empty());
        std::cout << "  PASS: max_results=0 -> no results\n";
    }

    // ── sorted list invariant ──
    {
        const auto &words = wl.words();
        for (size_t i = 1; i < words.size(); ++i)
            assert(words[i - 1] <= words[i]);
        std::cout << "  PASS: merged word list is sorted\n";
    }

    // ── no duplicates in merged list ──
    {
        const auto &words = wl.words();
        for (size_t i = 1; i < words.size(); ++i)
            assert(words[i - 1] != words[i]);
        std::cout << "  PASS: merged word list has no duplicates\n";
    }

    // ── tag suggest: all places ──
    {
        auto results = wl.suggest_by_tag("places", "", 50);
        assert(!results.empty());
        assert(results.size() >= 10);
        std::cout << "  PASS: tag 'places' all -> " << results.size() << " places\n";
    }

    // ── tag suggest: places with prefix ──
    {
        auto results = wl.suggest_by_tag("places", "አ", 10);
        assert(!results.empty());
        for (const auto &w : results)
            assert(w.size() >= 2 && w.rfind("አ", 0) == 0);
        std::cout << "  PASS: tag 'places' prefix 'አ' -> " << results.size() << " matches\n";
    }

    // ── tag suggest: unknown tag ──
    {
        auto results = wl.suggest_by_tag("unknown", "", 5);
        assert(results.empty());
        std::cout << "  PASS: unknown tag -> no results\n";
    }

    // ── tag suggest: places with no-match prefix ──
    {
        auto results = wl.suggest_by_tag("places", "ዘ", 5);
        assert(results.empty());
        std::cout << "  PASS: tag 'places' prefix 'ዘ' -> no match\n";
    }

    // ── tag suggest: people ──
    {
        auto results = wl.suggest_by_tag("people", "", 10);
        assert(!results.empty());
        std::cout << "  PASS: tag 'people' all -> " << results.size() << " people\n";
    }

    // ── tag suggest: organizations with max_results ──
    {
        auto results = wl.suggest_by_tag("organizations", "", 2);
        assert(results.size() == 2);
        std::cout << "  PASS: tag 'organizations' max=2 -> 2 results\n";
    }

    // ── top_words: returns first N from merged list ──
    {
        auto results = wl.top_words(5);
        assert(results.size() == 5);
        const auto &words = wl.words();
        for (size_t i = 0; i < 5; ++i)
            assert(results[i] == words[i]);
        std::cout << "  PASS: top_words(5) returns first 5 sorted words\n";
    }

    // ── top_words: max_results=0 ──
    {
        auto results = wl.top_words(0);
        assert(results.empty());
        std::cout << "  PASS: top_words(0) -> empty\n";
    }

    // ── top_words: request more than available ──
    {
        size_t n = wl.size() + 10;
        auto results = wl.top_words(n);
        assert(results.size() == wl.size());
        std::cout << "  PASS: top_words(oversize) caps at " << wl.size() << "\n";
    }

    // ── bigrams: load and check ──
    {
        ethio::WordList wl2;
        wl2.load(data_path());
        wl2.load_bigrams(bigrams_path());
        std::cout << "  PASS: bigrams loaded\n";

        auto next = wl2.suggest_next("ኢትዮጵያ", 5);
        assert(!next.empty());
        assert(next.size() <= 5);
        std::cout << "  PASS: suggest_next('ኢትዮጵያ') -> " << next.size() << " predictions\n";
    }

    // ── bigrams: suggest_next with unknown word ──
    {
        ethio::WordList wl2;
        wl2.load(data_path());
        wl2.load_bigrams(bigrams_path());

        auto next = wl2.suggest_next("የማይገኝ", 5);
        assert(next.empty());
        std::cout << "  PASS: suggest_next(unknown) -> empty\n";
    }

    // ── bigrams: suggest_next with empty previous ──
    {
        ethio::WordList wl2;
        wl2.load(data_path());
        wl2.load_bigrams(bigrams_path());

        auto next = wl2.suggest_next("", 5);
        assert(next.empty());
        std::cout << "  PASS: suggest_next('') -> empty\n";
    }

    // ── bigrams: suggest_next with max_results=0 ──
    {
        ethio::WordList wl2;
        wl2.load(data_path());
        wl2.load_bigrams(bigrams_path());

        auto next = wl2.suggest_next("ሰላም", 0);
        assert(next.empty());
        std::cout << "  PASS: suggest_next('ሰላም', 0) -> empty\n";
    }

    std::cout << "\nAll wordlist tests passed.\n";
    return 0;
}

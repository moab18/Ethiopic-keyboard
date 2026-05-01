#include "ethio/mapping.h"

#include <cassert>
#include <functional>
#include <iostream>

int main()
{
#ifdef DATA_DIR
    const std::string json_path = DATA_DIR "/amharic/am-sera.json";
#else
    const std::string json_path = "data/amharic/am-sera.json";
#endif

    auto mf = ethio::load_mapping_file(json_path);

    assert(!mf.states.empty());
    assert(mf.states[0].name == "init");
    std::cout << "Loaded: " << mf.title << " (" << mf.input_method << ")\n";

    auto &trie = mf.states[0].trie;
    auto *root = trie.root();

    // --- Test 1: Basic mapping "he" -> "ሀ" ---
    {
        auto *h_node = root->children.at("h").get();
        auto *he_node = h_node->children.at("e").get();
        assert(he_node->action.type == ethio::ActionType::Insert);
        assert(he_node->action.text == "ሀ");
        std::cout << "  PASS: 'he' -> 'ሀ'\n";
    }

    // --- Test 2: Bare consonant "h" -> "ህ" (terminal, but also has children) ---
    {
        auto *h_node = root->children.at("h").get();
        assert(h_node->action.type == ethio::ActionType::Insert);
        assert(h_node->action.text == "ህ");
        assert(!h_node->children.empty());
        std::cout << "  PASS: 'h' -> 'ህ' (terminal + branch node)\n";
    }

    // --- Test 3: Labiovelar "hWa" -> "ኋ" ---
    {
        auto *h_node = root->children.at("h").get();
        auto *hw_node = h_node->children.at("W").get();
        assert(hw_node->action.type == ethio::ActionType::None);
        auto *hwa_node = hw_node->children.at("a").get();
        assert(hwa_node->action.type == ethio::ActionType::Insert);
        assert(hwa_node->action.text == "ኋ");
        std::cout << "  PASS: 'hWa' -> 'ኋ'\n";
    }

    // --- Test 4: Commit delimiter "'" -> Commit action (has children for '' escape) ---
    {
        auto *quote_node = root->children.at("'").get();
        assert(quote_node->action.type == ethio::ActionType::Commit);
        assert(quote_node->action.text.empty());
        assert(quote_node->children.count("'") == 1);
        std::cout << "  PASS: \"'\" -> Commit (with '' child for escape)\n";
    }

    // --- Test 5: Literal apostrophe "''" -> Insert "'" ---
    {
        auto *q1 = root->children.at("'").get();
        auto *q2 = q1->children.at("'").get();
        assert(q2->action.type == ethio::ActionType::Insert);
        assert(q2->action.text == "'");
        std::cout << "  PASS: \"''\" -> Insert \"'\"\n";
    }

    // --- Test 6: Punctuation ":" -> "፡" (terminal, also has "::" child) ---
    {
        auto *colon_node = root->children.at(":").get();
        assert(colon_node->action.type == ethio::ActionType::Insert);
        assert(colon_node->action.text == "፡");
        assert(colon_node->children.count(":") == 1);
        std::cout << "  PASS: ':' -> '፡' (terminal + branch node)\n";
    }

    // --- Test 7: Double colon "::" -> "።" ---
    {
        auto *c1 = root->children.at(":").get();
        auto *c2 = c1->children.at(":").get();
        assert(c2->action.type == ethio::ActionType::Insert);
        assert(c2->action.text == "።");
        std::cout << "  PASS: '::' -> '።'\n";
    }

    // --- Test 8: Numeral "`1" -> "፩" ---
    {
        auto *backtick_node = root->children.at("`").get();
        auto *num1_node = backtick_node->children.at("1").get();
        assert(num1_node->action.type == ethio::ActionType::Insert);
        assert(num1_node->action.text == "፩");
        std::cout << "  PASS: '`1' -> '፩'\n";
    }

    // --- Test 9: Slash prefix "/he" -> "ኀ" ---
    {
        auto *slash_node = root->children.at("/").get();
        auto *slash_h = slash_node->children.at("h").get();
        auto *slash_he = slash_h->children.at("e").get();
        assert(slash_he->action.type == ethio::ActionType::Insert);
        assert(slash_he->action.text == "ኀ");
        std::cout << "  PASS: '/he' -> 'ኀ'\n";
    }

    // --- Test 10: Self-escape "//" -> "/" ---
    {
        auto *slash_node = root->children.at("/").get();
        assert(slash_node->action.type == ethio::ActionType::None);
        auto *slash_slash = slash_node->children.at("/").get();
        assert(slash_slash->action.type == ethio::ActionType::Insert);
        assert(slash_slash->action.text == "/");
        std::cout << "  PASS: '//' -> '/'\n";
    }

    // --- Test 11: Multi-char value "`1000" -> "፲፻" ---
    {
        auto *bt = root->children.at("`").get();
        auto *bt1 = bt->children.at("1").get();
        auto *bt10 = bt1->children.at("0").get();
        auto *bt100 = bt10->children.at("0").get();
        auto *bt1000 = bt100->children.at("0").get();
        assert(bt1000->action.type == ethio::ActionType::Insert);
        assert(bt1000->action.text == "፲፻");
        std::cout << "  PASS: '`1000' -> '፲፻' (multi-char value)\n";
    }

    // --- Test 12: Count terminal Insert nodes ---
    {
        int total = 0;
        std::function<void(const ethio::TrieNode *)> count;
        count = [&](const ethio::TrieNode *node) {
            if (node->action.type == ethio::ActionType::Insert)
                total++;
            for (auto &[k, v] : node->children)
                count(v.get());
        };
        count(root);
        std::cout << "  Total Insert nodes: " << total << '\n';
        assert(total > 300);
    }

    std::cout << "\nAll tests passed.\n";
    return 0;
}

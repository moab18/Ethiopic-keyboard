#pragma once

#include "ethio/mapping.h"

#include <string>
#include <string_view>

namespace ethio {

class Engine {
public:
    Engine() = default;

    void load_trie(const Trie &trie);

    bool filter(std::string_view key);

    std::string flush();

    std::string_view composing() const
    {
        return pending_text_.empty() ? composing_ : pending_text_;
    }

    void reset();

private:
    bool descend(std::string_view key, const TrieNode *node);
    bool try_key_from_root(std::string_view key);

    const TrieNode *trie_root_ = nullptr;
    const TrieNode *current_node_ = nullptr;
    std::string composing_;
    std::string pending_text_;
    std::string produced_;
};

} // namespace ethio

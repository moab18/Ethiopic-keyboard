#pragma once

#include "ethio/mapping.h"

#include <string>
#include <string_view>

namespace ethio {

inline size_t utf8_codepoint_count(std::string_view s)
{
    size_t count = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
            count++;
    }
    return count;
}

inline size_t cp_offset_to_byte(std::string_view s, size_t cp_offset)
{
    size_t byte = 0;
    size_t cp = 0;
    while (cp < cp_offset && byte < s.size()) {
        if ((static_cast<unsigned char>(s[byte]) & 0xC0) != 0x80)
            cp++;
        byte++;
    }
    while (byte < s.size() && (static_cast<unsigned char>(s[byte]) & 0xC0) == 0x80)
        byte++;
    return byte;
}

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

    size_t cursor() const { return cursor_pos_; }

    void move_cursor_left();
    void move_cursor_right();
    void move_cursor_home();
    void move_cursor_end();

    void reset();

    void finish_composition();
    std::string_view produced_text() const { return produced_; }

    void append_produced(std::string_view s) { produced_.append(s); }

    bool passthrough() const { return passthrough_; }
    void toggle_passthrough() { passthrough_ = !passthrough_; }

private:
    bool descend(std::string_view key, const TrieNode *node);
    bool try_key_from_root(std::string_view key);

    const TrieNode *trie_root_ = nullptr;
    const TrieNode *current_node_ = nullptr;
    std::string composing_;
    std::string pending_text_;
    std::string produced_;
    size_t cursor_pos_ = 0;
    bool passthrough_ = false;
};

} // namespace ethio

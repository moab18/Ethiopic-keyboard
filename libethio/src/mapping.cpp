#include "ethio/mapping.h"

#include <fstream>
#include <stdexcept>

#include "ethio/json.hpp"

using json = nlohmann::json;

namespace ethio {

static std::vector<std::string> split_utf8(std::string_view s)
{
    std::vector<std::string> chars;
    chars.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        size_t len = 1;
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0x80u) == 0) {
            len = 1;
        } else if ((c & 0xE0u) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0u) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8u) == 0xF0) {
            len = 4;
        }
        if (i + len > s.size()) break;
        chars.emplace_back(s.substr(i, len));
        i += len;
    }
    return chars;
}

void Trie::insert(std::string_view key, std::string_view value)
{
    auto chars = split_utf8(key);
    if (chars.empty()) return;

    TrieNode *node = root_.get();
    for (const auto &ch : chars) {
        auto &child = node->children[ch];
        if (!child) {
            child = std::make_unique<TrieNode>();
        }
        node = child.get();
    }
    node->action.type = ActionType::Insert;
    node->action.text = std::string(value);
}

void Trie::insert_commit(std::string_view key)
{
    auto chars = split_utf8(key);
    if (chars.empty()) return;

    TrieNode *node = root_.get();
    for (const auto &ch : chars) {
        auto &child = node->children[ch];
        if (!child) {
            child = std::make_unique<TrieNode>();
        }
        node = child.get();
    }
    node->action.type = ActionType::Commit;
    node->action.text.clear();
}

MappingFile load_mapping_file(const std::string &json_path)
{
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open mapping file: " + json_path);
    }

    json j;
    ifs >> j;

    MappingFile mf;
    mf.input_method = j.value("input_method", "");
    mf.title = j.value("title", "");
    mf.description = j.value("description", "");
    mf.version = j.value("version", "");
    mf.based_on = j.value("based_on", "");

    const auto &states = j["states"];
    for (auto it = states.begin(); it != states.end(); ++it) {
        MappingState state;
        state.name = it.key();

        const auto &map_entries = it.value()["map"];
        for (auto mit = map_entries.begin(); mit != map_entries.end(); ++mit) {
            std::string key = mit.key();
            std::string val = mit.value();

            if (val.empty()) {
                state.trie.insert_commit(key);
            } else {
                state.trie.insert(key, val);
            }
        }

        mf.states.push_back(std::move(state));
    }

    return mf;
}

} // namespace ethio

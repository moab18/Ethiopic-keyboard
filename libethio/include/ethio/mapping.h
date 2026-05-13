// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ethio {

enum class ActionType {
    None,
    Insert,
    Commit,
};

struct Action {
    ActionType type = ActionType::None;
    std::string text;

    bool is_terminal() const { return type != ActionType::None; }
};

struct TrieNode {
    std::unordered_map<std::string, std::unique_ptr<TrieNode>> children;
    Action action;
};

class Trie {
public:
    Trie() : root_(std::make_unique<TrieNode>()) {}

    void insert(std::string_view key, std::string_view value);

    void insert_commit(std::string_view key);

    TrieNode* root() { return root_.get(); }
    const TrieNode* root() const { return root_.get(); }

    bool empty() const { return root_->children.empty(); }

    size_t root_entry_count() const { return root_->children.size(); }

private:
    std::unique_ptr<TrieNode> root_;
};

struct MappingState {
    std::string name;
    Trie trie;
};

struct MappingFile {
    std::string input_method;
    std::string title;
    std::string description;
    std::string version;
    std::string based_on;
    std::vector<MappingState> states;
};

MappingFile load_mapping_file(const std::string &json_path);

} // namespace ethio

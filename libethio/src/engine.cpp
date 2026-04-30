#include "ethio/engine.h"

namespace ethio {

void Engine::load_trie(const Trie &trie)
{
    trie_root_ = trie.root();
    reset();
}

void Engine::reset()
{
    current_node_ = trie_root_;
    composing_.clear();
    pending_text_.clear();
}

std::string Engine::flush()
{
    std::string result;
    result.swap(produced_);
    return result;
}

bool Engine::filter(std::string_view key)
{
    if (!trie_root_) return false;

    if (current_node_ == nullptr || current_node_ == trie_root_) {
        if (current_node_ == nullptr) {
            current_node_ = trie_root_;
        }
        return try_key_from_root(key);
    }

    auto it = current_node_->children.find(std::string(key));
    if (it != current_node_->children.end()) {
        return descend(key, it->second.get());
    }

    if (!pending_text_.empty()) {
        produced_ += pending_text_;
        pending_text_.clear();
    }
    reset();
    return try_key_from_root(key);
}

bool Engine::try_key_from_root(std::string_view key)
{
    if (!trie_root_) return false;

    auto it = trie_root_->children.find(std::string(key));
    if (it == trie_root_->children.end()) {
        return false;
    }

    return descend(key, it->second.get());
}

bool Engine::descend(std::string_view key, const TrieNode *node)
{
    composing_.append(key);
    current_node_ = node;

    if (current_node_->action.type == ActionType::Insert) {
        pending_text_ = current_node_->action.text;
    } else {
        pending_text_.clear();
    }

    if (current_node_->children.empty()) {
        if (!pending_text_.empty()) {
            produced_ += pending_text_;
            pending_text_.clear();
        }
        reset();
    }

    return true;
}

} // namespace ethio

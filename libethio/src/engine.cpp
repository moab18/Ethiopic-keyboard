#include "ethio/engine.h"

#include <cstdio>

namespace ethio {

void Engine::load_trie(const Trie &trie)
{
    trie_root_ = trie.root();
    fprintf(stderr, "DEBUG ethio::Engine::load_trie: root=%p entries=%zu\n",
            static_cast<const void *>(trie_root_),
            trie_root_ ? trie_root_->children.size() : 0);
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
    if (passthrough_) {
        fprintf(stderr, "DEBUG ethio::Engine::filter: passthrough mode, returning false\n");
        return false;
    }

    if (!trie_root_) {
        fprintf(stderr, "DEBUG ethio::Engine::filter: no trie loaded, returning false\n");
        return false;
    }

    if (current_node_ == nullptr || current_node_ == trie_root_) {
        if (current_node_ == nullptr) {
            current_node_ = trie_root_;
        }
        fprintf(stderr, "DEBUG ethio::Engine::filter: at root, trying key='%s'\n",
                std::string(key).c_str());
        return try_key_from_root(key);
    }

    fprintf(stderr, "DEBUG ethio::Engine::filter: at node depth=%zu composing='%s', "
            "looking up key='%s'\n",
            composing_.size(), composing_.c_str(),
            std::string(key).c_str());

    auto it = current_node_->children.find(std::string(key));
    if (it != current_node_->children.end()) {
        return descend(key, it->second.get());
    }

    fprintf(stderr, "DEBUG ethio::Engine::filter: key='%s' NOT found, "
            "flushing pending='%s' and retrying from root\n",
            std::string(key).c_str(), pending_text_.c_str());

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

    bool has_children = !current_node_->children.empty();

    if (current_node_->action.type == ActionType::Insert) {
        pending_text_ = current_node_->action.text;
        fprintf(stderr, "DEBUG ethio::Engine::descend: key='%s' composing='%s' "
                "action=Insert text='%s' children=%s\n",
                std::string(key).c_str(), composing_.c_str(),
                pending_text_.c_str(),
                has_children ? "yes" : "no");
    } else if (current_node_->action.type == ActionType::Commit) {
        pending_text_.clear();
        fprintf(stderr, "DEBUG ethio::Engine::descend: key='%s' composing='%s' "
                "action=Commit children=%s\n",
                std::string(key).c_str(), composing_.c_str(),
                has_children ? "yes" : "no");
    } else {
        pending_text_.clear();
        fprintf(stderr, "DEBUG ethio::Engine::descend: key='%s' composing='%s' "
                "action=None children=%s\n",
                std::string(key).c_str(), composing_.c_str(),
                has_children ? "yes" : "no");
    }

    if (!has_children) {
        if (!pending_text_.empty()) {
            produced_ += pending_text_;
            pending_text_.clear();
            fprintf(stderr, "DEBUG ethio::Engine::descend: leaf node -> auto-commit '%s', "
                    "composing cleared\n", produced_.c_str());
        }
        reset();
    }

    return true;
}

} // namespace ethio

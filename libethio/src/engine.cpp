#include "ethio/engine.h"
#include "ethio/logger.h"

#include <cstdio>
#include <string>

namespace ethio {

void Engine::load_trie(const Trie &trie)
{
    trie_root_ = trie.root();
    logger.debug("ethio::Engine::load_trie: root=%p entries=%zu",
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
        logger.debug("ethio::Engine::filter: passthrough mode, returning false");
        return false;
    }

    if (!trie_root_) {
        logger.debug("ethio::Engine::filter: no trie loaded, returning false");
        return false;
    }

    if (current_node_ == nullptr || current_node_ == trie_root_) {
        if (current_node_ == nullptr) {
            current_node_ = trie_root_;
        }
        std::string key_str(key);
        logger.debug("ethio::Engine::filter: at root, trying key='%s'",
                key_str.c_str());
        return try_key_from_root(key);
    }

    std::string key_str(key);
    logger.debug("ethio::Engine::filter: at node depth=%zu composing='%s', "
            "looking up key='%s'",
            composing_.size(), composing_.c_str(),
            key_str.c_str());

    auto it = current_node_->children.find(key_str);
    if (it != current_node_->children.end()) {
        return descend(key, it->second.get());
    }

    logger.debug("ethio::Engine::filter: key='%s' NOT found, "
            "flushing pending='%s' and retrying from root",
            key_str.c_str(), pending_text_.c_str());

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

    std::string key_str(key);
    auto it = trie_root_->children.find(key_str);
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
    std::string key_str(key);

    if (current_node_->action.type == ActionType::Insert) {
        pending_text_ = current_node_->action.text;
        logger.debug("ethio::Engine::descend: key='%s' composing='%s' "
                "action=Insert text='%s' children=%s",
                key_str.c_str(), composing_.c_str(),
                pending_text_.c_str(),
                has_children ? "yes" : "no");
    } else if (current_node_->action.type == ActionType::Commit) {
        pending_text_.clear();
        logger.debug("ethio::Engine::descend: key='%s' composing='%s' "
                "action=Commit children=%s",
                key_str.c_str(), composing_.c_str(),
                has_children ? "yes" : "no");
    } else {
        pending_text_.clear();
        logger.debug("ethio::Engine::descend: key='%s' composing='%s' "
                "action=None children=%s",
                key_str.c_str(), composing_.c_str(),
                has_children ? "yes" : "no");
    }

    if (!has_children) {
        if (!pending_text_.empty()) {
            produced_ += pending_text_;
            pending_text_.clear();
            logger.debug("ethio::Engine::descend: leaf node -> auto-commit '%s', "
                    "composing cleared", produced_.c_str());
        }
        reset();
    }

    return true;
}

} // namespace ethio

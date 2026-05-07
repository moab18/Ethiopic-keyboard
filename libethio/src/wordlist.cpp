#include "ethio/wordlist.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "ethio/json.hpp"

namespace ethio {

void WordList::load(const std::string &json_path)
{
    std::ifstream ifs(json_path);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open word list file: " + json_path);

    Json root;
    ifs >> root;

    categories_.clear();
    merged_.clear();

    for (auto it = root.begin(); it != root.end(); ++it) {
        const Json &arr = it.value();
        if (!arr.is_array()) continue;

        std::vector<std::string> words;
        words.reserve(arr.arr_size());
        for (size_t i = 0; i < arr.arr_size(); ++i) {
            std::string w = arr[i];
            if (!w.empty()) {
                words.push_back(w);
                merged_.push_back(w);
            }
        }
        if (!words.empty())
            categories_[it.key()] = std::move(words);
    }

    std::sort(merged_.begin(), merged_.end());
    merged_.erase(std::unique(merged_.begin(), merged_.end()), merged_.end());
}

std::vector<std::string> WordList::suggest(std::string_view prefix,
                                           size_t max_results) const
{
    std::vector<std::string> results;
    if (prefix.empty() || max_results == 0) return results;

    auto it = std::lower_bound(merged_.begin(), merged_.end(), prefix,
        [](const std::string &w, std::string_view p) {
            return w < p;
        });

    for (; it != merged_.end() && results.size() < max_results; ++it) {
        if (it->size() < prefix.size()) break;
        if (it->compare(0, prefix.size(), prefix) != 0) break;
        results.push_back(*it);
    }

    return results;
}

std::vector<std::string> WordList::suggest_by_tag(std::string_view tag,
                                                  std::string_view prefix,
                                                  size_t max_results) const
{
    std::vector<std::string> results;

    auto cat_it = categories_.find(std::string(tag));
    if (cat_it == categories_.end()) return results;

    const auto &words = cat_it->second;

    if (prefix.empty()) {
        for (const auto &w : words) {
            if (results.size() >= max_results) break;
            results.push_back(w);
        }
        return results;
    }

    for (const auto &w : words) {
        if (results.size() >= max_results) break;
        if (w.size() >= prefix.size() && w.compare(0, prefix.size(), prefix) == 0)
            results.push_back(w);
    }

    return results;
}

std::vector<std::string> WordList::top_words(size_t max_results) const
{
    std::vector<std::string> results;
    for (const auto &w : merged_) {
        if (results.size() >= max_results) break;
        results.push_back(w);
    }
    return results;
}

std::vector<std::string> WordList::suggest_next(std::string_view previous_word,
                                                 size_t max_results) const
{
    std::vector<std::string> results;
    if (previous_word.empty() || max_results == 0) return results;

    auto it = bigrams_.find(std::string(previous_word));
    if (it == bigrams_.end()) return results;

    for (const auto &w : it->second) {
        if (results.size() >= max_results) break;
        results.push_back(w);
    }
    return results;
}

void WordList::load_bigrams(const std::string &json_path)
{
    std::ifstream ifs(json_path);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open bigram file: " + json_path);

    Json root;
    ifs >> root;

    bigrams_.clear();

    for (auto it = root.begin(); it != root.end(); ++it) {
        const Json &arr = it.value();
        if (!arr.is_array()) continue;

        std::vector<std::string> next_words;
        next_words.reserve(arr.arr_size());
        for (size_t i = 0; i < arr.arr_size(); ++i) {
            std::string w = arr[i];
            if (!w.empty())
                next_words.push_back(w);
        }
        if (!next_words.empty())
            bigrams_[it.key()] = std::move(next_words);
    }
}

} // namespace ethio

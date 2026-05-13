// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ethio {

class WordList {
public:
    void load(const std::string &json_path);

    void load_bigrams(const std::string &json_path);

    std::vector<std::string> suggest(std::string_view prefix,
                                     size_t max_results = 5) const;

    std::vector<std::string> suggest_by_tag(std::string_view tag,
                                            std::string_view prefix,
                                            size_t max_results = 5) const;

    std::vector<std::string> top_words(size_t max_results = 5) const;

    std::vector<std::string> suggest_next(std::string_view previous_word,
                                          size_t max_results = 5) const;

    size_t size() const { return merged_.size(); }

    bool has_tag(std::string_view tag) const
    {
        return categories_.find(std::string(tag)) != categories_.end();
    }

    const std::vector<std::string> &words() const { return merged_; }

private:
    std::unordered_map<std::string, std::vector<std::string>> categories_;
    std::vector<std::string> merged_;
    std::unordered_map<std::string, std::vector<std::string>> bigrams_;
};

} // namespace ethio

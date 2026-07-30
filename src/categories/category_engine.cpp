#include "categories/category_engine.h"

#include <algorithm>
#include <regex>
#include <mutex>

namespace remo {
namespace categories {

class CategoryEngine::Impl {
public:
    std::mutex mutex;
    std::vector<Category> categories;
    std::vector<CategoryRule> rules;
};

CategoryEngine::CategoryEngine()
    : d(std::make_unique<Impl>())
{
}

CategoryEngine::~CategoryEngine() = default;

int64_t CategoryEngine::addCategory(const std::string& name, const std::string& savePath, int64_t parentId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    Category cat;
    cat.id = d->categories.empty() ? 1 : d->categories.back().id + 1;
    cat.name = name;
    cat.savePath = savePath;
    cat.parentCategoryId = parentId;
    d->categories.push_back(cat);
    return cat.id;
}

bool CategoryEngine::removeCategory(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = std::remove_if(d->categories.begin(), d->categories.end(),
                             [id](const Category& c) { return c.id == id; });
    if (it != d->categories.end()) {
        d->categories.erase(it, d->categories.end());
        return true;
    }
    return false;
}

std::vector<Category> CategoryEngine::getAllCategories() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->categories;
}

Category CategoryEngine::getCategory(int64_t id) const {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (const auto& cat : d->categories) {
        if (cat.id == id) {
            return cat;
        }
    }
    return Category{};
}

int64_t CategoryEngine::addRule(int64_t categoryId, const std::string& ruleType, const std::string& pattern) {
    std::lock_guard<std::mutex> lock(d->mutex);
    CategoryRule rule;
    rule.id = d->rules.empty() ? 1 : d->rules.back().id + 1;
    rule.categoryId = categoryId;
    rule.ruleType = ruleType;
    rule.pattern = pattern;
    rule.isActive = true;
    d->rules.push_back(rule);
    return rule.id;
}

bool CategoryEngine::removeRule(int64_t ruleId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = std::remove_if(d->rules.begin(), d->rules.end(),
                             [ruleId](const CategoryRule& r) { return r.id == ruleId; });
    if (it != d->rules.end()) {
        d->rules.erase(it, d->rules.end());
        return true;
    }
    return false;
}

std::vector<CategoryRule> CategoryEngine::getRulesForCategory(int64_t categoryId) const {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::vector<CategoryRule> result;
    for (const auto& rule : d->rules) {
        if (rule.categoryId == categoryId) {
            result.push_back(rule);
        }
    }
    return result;
}

std::string CategoryEngine::classify(const std::string& filename, const std::string& url) const {
    std::string category = determineCategory(filename, url);
    return category;
}

std::string CategoryEngine::getSavePath(int64_t categoryId) const {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (const auto& cat : d->categories) {
        if (cat.id == categoryId) {
            return cat.savePath;
        }
    }
    return "";
}

std::string CategoryEngine::determineCategory(const std::string& filename, const std::string& url) const {
    std::lock_guard<std::mutex> lock(d->mutex);

    for (const auto& rule : d->rules) {
        if (!rule.isActive) {
            continue;
        }

        if (rule.ruleType == "extension") {
            size_t dotPos = filename.rfind('.');
            if (dotPos != std::string::npos) {
                std::string ext = filename.substr(dotPos + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                std::string pattern = rule.pattern;
                std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);
                if (ext == pattern) {
                    for (const auto& cat : d->categories) {
                        if (cat.id == rule.categoryId) {
                            return cat.name;
                        }
                    }
                }
            }
        } else if (rule.ruleType == "domain") {
            if (url.find(rule.pattern) != std::string::npos) {
                for (const auto& cat : d->categories) {
                    if (cat.id == rule.categoryId) {
                        return cat.name;
                    }
                }
            }
        } else if (rule.ruleType == "regex") {
            try {
                std::regex re(rule.pattern);
                if (std::regex_search(filename, re) || std::regex_search(url, re)) {
                    for (const auto& cat : d->categories) {
                        if (cat.id == rule.categoryId) {
                            return cat.name;
                        }
                    }
                }
            } catch (const std::regex_error&) {
            }
        }
    }

    return "Default";
}

} // namespace categories
} // namespace remo

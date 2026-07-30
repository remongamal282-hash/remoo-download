#ifndef REMO_DOWNLOAD_CATEGORIES_CATEGORY_ENGINE_H
#define REMO_DOWNLOAD_CATEGORIES_CATEGORY_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace categories {

struct Category {
    int64_t id = 0;
    std::string name;
    std::string savePath;
    int64_t parentCategoryId = -1;
    bool isDefault = false;
};

struct CategoryRule {
    int64_t id = 0;
    int64_t categoryId = 0;
    std::string ruleType;
    std::string pattern;
    bool isActive = true;
};

enum class RuleType { Extension, Domain, Regex };

class CategoryEngine {
public:
    CategoryEngine();
    ~CategoryEngine();

    int64_t addCategory(const std::string& name, const std::string& savePath, int64_t parentId = -1);
    bool removeCategory(int64_t id);
    std::vector<Category> getAllCategories() const;
    Category getCategory(int64_t id) const;

    int64_t addRule(int64_t categoryId, const std::string& ruleType, const std::string& pattern);
    bool removeRule(int64_t ruleId);
    std::vector<CategoryRule> getRulesForCategory(int64_t categoryId) const;

    std::string classify(const std::string& filename, const std::string& url) const;
    std::string getSavePath(int64_t categoryId) const;
    std::string determineCategory(const std::string& filename, const std::string& url) const;

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace categories
} // namespace remo

#endif // REMO_DOWNLOAD_CATEGORIES_CATEGORY_ENGINE_H
#ifndef DATABASE_H
#define DATABASE_H
#include <string>
#include <optional>
#include <unordered_map>

class Database {
public:
    void set(std::string key, std::string value);

    std::optional<std::string> get(
        const std::string& key
    ) const;

    bool erase(const std::string& key);

private:
    std::unordered_map<std::string, std::string> cache_;
};

#endif
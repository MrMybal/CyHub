#include "hub.hpp"

#include <fstream>

namespace cyhub {
std::string load_language(const fs::path& file) {
    if (!file.empty()) {
        try {
            const auto language = read_json(file, 4096).value("language", "en");
            if (language == "fr")
                return language;
        } catch (const std::exception&) {
            // Missing or invalid settings use the first-run language.
        }
    }
    return "en";
}

void save_language(const fs::path& file, std::string_view language) {
    if (file.empty())
        throw std::runtime_error("The preferences directory is unavailable.");
    auto pending = file;
    pending += "." + std::to_string(cymcp::Clock::now().time_since_epoch().count()) + ".tmp";
    try {
        std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
        stream << Json{{"language", language}}.dump() << '\n';
        stream.close();
        if (!stream)
            throw std::runtime_error("Could not save the language preference.");
        fs::rename(pending, file);
    } catch (...) {
        std::error_code ignored;
        fs::remove(pending, ignored);
        throw;
    }
}
}

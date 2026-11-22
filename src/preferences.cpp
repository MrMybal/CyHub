#include "hub.hpp"

#include <fstream>

namespace cyhub {
Preferences load_preferences(const fs::path& file) {
    if (!file.empty()) {
        try {
            const auto data = read_json(file, 32768);
            return {data.value("language", "en") == "fr" ? "fr" : "en",
                    data.value("downloadDirectory", ""), data.value("checkOnStart", true),
                    data.value("cards", false)};
        } catch (const std::exception&) {
            // Missing or invalid settings use the first-run language.
        }
    }
    return {};
}

void save_preferences(const fs::path& file, const Preferences& preferences) {
    if (file.empty())
        throw std::runtime_error("The preferences directory is unavailable.");
    auto pending = file;
    pending += "." + std::to_string(cymcp::Clock::now().time_since_epoch().count()) + ".tmp";
    try {
        std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
        stream << Json{{"language", preferences.language}, {"downloadDirectory", preferences.download_directory},
                       {"checkOnStart", preferences.check_on_start}, {"cards", preferences.cards}}.dump() << '\n';
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

std::string load_language(const fs::path& file) { return load_preferences(file).language; }

void save_language(const fs::path& file, std::string_view language) {
    auto preferences = load_preferences(file);
    preferences.language = language;
    save_preferences(file, preferences);
}
}

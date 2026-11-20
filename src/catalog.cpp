#include "hub.hpp"

#include <algorithm>
#include <fstream>
#include <set>

namespace cyhub {
namespace {
bool letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool digit(char c) {
    return c >= '0' && c <= '9';
}
bool token(std::string_view value, std::string_view extra) {
    return !value.empty() && std::ranges::all_of(value, [extra](char c) {
        return letter(c) || digit(c) || extra.find(c) != std::string_view::npos;
    });
}
std::string read_text(const fs::path& file, std::size_t limit) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Fichier illisible : " + cymcp::path_utf8(file));
    std::string data(limit + 1, '\0');
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (stream.bad() || static_cast<std::size_t>(stream.gcount()) > limit)
        throw std::runtime_error("Fichier trop volumineux ou illisible.");
    data.resize(static_cast<std::size_t>(stream.gcount()));
    return data;
}
}

Json read_json(const fs::path& file, std::size_t limit) {
    return cymcp::parse_json(read_text(file, limit), limit);
}

std::string read_skill(const fs::path& directory, const Project& project) {
    if (!valid_id(project.skill) || !portable_path(project.skill))
        throw std::runtime_error("Ce projet n’a aucun skill dans CyHub.");
    return read_text(directory / "skills" / project.skill / "SKILL.md", 16 * 1024);
}

bool valid_id(std::string_view value) {
    return value.size() <= 80 && token(value, "-_") && value.front() != '-';
}

bool valid_digest(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char c) {
        return digit(c) || (c >= 'a' && c <= 'f');
    });
}

std::string fold(std::string_view value) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == 0xc3 && i + 1 < value.size()) {
            const auto code = static_cast<unsigned char>(value[i + 1]) | 0x20;
            const auto index = static_cast<int>(code) - 0xa0;
            constexpr std::string_view latin = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";
            if (index >= 0 && index < static_cast<int>(latin.size()) && latin[index] != '/') {
                result += latin[index];
                ++i;
                continue;
            }
        }
        result += c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
    }
    return result;
}

bool portable_path(std::string_view value) {
    if (value.empty() || value.size() > 220)
        return false;
    while (!value.empty()) {
        const auto slash = value.find('/');
        const auto part = value.substr(0, slash);
        if (part.empty() || part.size() > 120 || part.front() == ' ' ||
            part.back() == ' ' || part.back() == '.' ||
            part.find_first_of("<>:\"\\|?*") != std::string_view::npos ||
            std::ranges::any_of(part, [](unsigned char c) { return c < 32 || c == 127; }))
            return false;
        const auto stem = fold(part.substr(0, part.find('.')));
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
            stem == "conin$" || stem == "conout$" ||
            ((stem.starts_with("com") || stem.starts_with("lpt")) &&
             ((stem.size() == 4 && digit(stem.back())) ||
              stem.ends_with("¹") || stem.ends_with("²") || stem.ends_with("³"))))
            return false;
        if (slash == std::string_view::npos)
            return true;
        value.remove_prefix(slash + 1);
    }
    return false;
}

Catalog load_catalog(const fs::path& file) {
    const auto data = read_json(file, 512 * 1024);
    const auto& entries = data.at("projects");
    if (!entries.is_array() || entries.empty() || entries.size() > 300)
        throw std::runtime_error("Le catalogue doit contenir entre 1 et 300 projets.");

    Catalog result;
    std::set<std::string> ids;
    for (const auto& entry : entries) {
        Project project{entry.at("id"), entry.at("name"), entry.at("category"),
                        entry.at("description"), entry.value("repo", ""), {},
                        entry.value("skill", ""), entry.value("includePrerelease", false)};
        if (!valid_id(project.id) || !portable_path(project.id) || project.id != fold(project.id) ||
            !ids.insert(project.id).second || project.name.empty() || project.name.size() > 80 ||
            project.category.empty() || project.category.size() > 60 || project.description.size() > 800)
            throw std::runtime_error("Projet invalide ou identifiant en double : " + project.id);
        if (!project.skill.empty() && (!valid_id(project.skill) || !portable_path(project.skill) ||
            project.skill != fold(project.skill) || project.skill.front() == '_' ||
            project.skill.find('_') != std::string::npos || project.skill.size() > 64))
            throw std::runtime_error("Identifiant de skill invalide : " + project.skill);

        if (!project.repo.empty()) {
            const auto slash = project.repo.find('/');
            if (slash == std::string::npos || !token(project.repo.substr(0, slash), "-") ||
                !token(project.repo.substr(slash + 1), "-_.") ||
                !portable_path(project.repo.substr(slash + 1)))
                throw std::runtime_error("Dépôt GitHub invalide : " + project.repo);
        }
        if (entry.contains("windowsX64")) {
            const auto& spec = entry.at("windowsX64");
            Package p{spec.at("asset"), spec.at("entry")};
            const auto zip = p.asset.ends_with(".zip");
            const auto exe = p.asset.ends_with(".exe");
            const auto wheel = p.asset.ends_with(".whl");
            if (project.repo.empty() || !portable_path(p.asset) || !portable_path(p.entry) ||
                !token(p.asset, "-_.{}") || (!zip && !exe && !wheel) ||
                ((zip || exe) && !fold(p.entry).ends_with(".exe")) ||
                (wheel && (!p.entry.ends_with(".whl") || p.entry.find('/') != std::string::npos)) ||
                (exe && p.entry.find('/') != std::string::npos))
                throw std::runtime_error("Paquet Windows invalide : " + project.id);
            project.windows_x64 = std::move(p);
        }
        result.push_back(std::move(project));
    }
    return result;
}

const Project* find_project(const Catalog& catalog, std::string_view id) {
    const auto it = std::ranges::find(catalog, id, &Project::id);
    return it == catalog.end() ? nullptr : &*it;
}

std::string Project::url() const {
    return repo.empty() ? "" : "https://github.com/" + repo;
}

bool matches(const Project& project, std::string_view query) {
    const auto text = fold(project.name + " " + project.category + " " + project.description + " " + project.repo);
    while (!query.empty()) {
        const auto space = query.find(' ');
        if (text.find(fold(query.substr(0, space))) == std::string::npos)
            return false;
        if (space == std::string_view::npos)
            break;
        query.remove_prefix(space + 1);
    }
    return true;
}
}

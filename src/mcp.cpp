#include "hub.hpp"
#include "companion.hpp"
#include "installed_apps.hpp"

#include <cymcp/mcp.hpp>
#include <iostream>

int main() {
    using namespace cyhub;
    try {
        const auto directory = executable_directory();
        const auto catalog = load_catalog(directory / "catalog.json");
        const auto root = managed_root();
        const auto local_companions = companion_root();
        const auto local_projects = user_projects_root();
        cymcp::Registry operations(cymcp::Policy::allow_all());
        const Json output_schema = {{"type", "object"}};
        const auto describe = [&](const Project& project, const InstallationInventory& inventory) {
            auto installed = installed_version(is_companion(project) ? local_companions : root, project);
            if (!installed && !is_companion(project))
                installed = installed_version(local_projects, project);
            const auto detected = inventory.find(project.id);
            const auto version = installed ? installed->version :
                detected != inventory.end() ? detected->second.version : "";
            return Json{{"id", project.id}, {"name", project.name}, {"category", project.category},
                        {"description", project.description}, {"repository", project.url()},
                        {"windowsManagedPackage", project.windows_x64.has_value()},
                        {"skill", project.skill.empty() ? Json(nullptr) : Json(project.skill)},
                        {"installed", installed.has_value() || detected != inventory.end()},
                        {"installationSource", installed ? Json("cyhub") :
                            detected != inventory.end() ? Json("windows") : Json(nullptr)},
                        {"installedVersion", version.empty() ? Json(nullptr) : Json(version)}};
        };

        operations.add({
            "list_projects", "List CyHub projects, detected installations and installed versions.",
            {{"type", "object"}, {"additionalProperties", false},
             {"properties", {{"query", {{"type", "string"}, {"maxLength", 120}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext& ctx) {
                const auto inventory = detect_installations(catalog, ctx.cancellation);
                auto projects = Json::array();
                for (const auto& project : catalog) {
                    if (matches(project, args.value("query", "")))
                        projects.push_back(describe(project, inventory));
                }
                return Json{{"projects", projects}};
            }});

        operations.add({
            "check_project_update", "Compare the latest accessible GitHub release with the installed version.",
            {{"type", "object"}, {"additionalProperties", false}, {"required", {"id"}},
             {"properties", {{"id", {{"type", "string"}, {"maxLength", 80}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext& ctx) {
                const auto* project = find_project(catalog, args.at("id").get<std::string>());
                if (!project)
                    throw cymcp::Error("NotFound", "Unknown project ID");
                const auto inventory = detect_installations({*project}, ctx.cancellation);
                auto info = describe(*project, inventory);
                const auto& installed = info.at("installedVersion");
                const auto release = discover_release(*project, ctx.cancellation);
                const bool update = release && installed.is_string() &&
                    newer_version(installed.get_ref<const std::string&>(), release->tag);
                return Json{{"project", std::move(info)},
                            {"latestVersion", release ? Json(release->tag) : Json(nullptr)},
                            {"releaseUrl", release ? Json(release->url) : Json(nullptr)},
                            {"managedAssetAvailable", release && release->asset.has_value()},
                            {"updateAvailable", update}};
            }});

        operations.add({
            "list_project_skills", "Find the CyMCPSDK integration skills shipped with CyHub.",
            {{"type", "object"}, {"additionalProperties", false},
             {"properties", {{"query", {{"type", "string"}, {"maxLength", 120}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext&) {
                auto skills = Json::array();
                for (const auto& project : catalog) {
                    if (!project.skill.empty() && matches(project, args.value("query", "")))
                        skills.push_back({{"id", project.id}, {"project", project.name}, {"skill", project.skill}});
                }
                return Json{{"skills", skills}};
            }});

        operations.add({
            "get_project_skill", "Read the bundled SKILL.md for a project from list_project_skills.",
            {{"type", "object"}, {"additionalProperties", false}, {"required", {"id"}},
             {"properties", {{"id", {{"type", "string"}, {"maxLength", 80}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext&) {
                const auto* project = find_project(catalog, args.at("id").get<std::string>());
                if (!project || project->skill.empty())
                    throw cymcp::Error("NotFound", "No skill for this project ID");
                return Json{{"id", project->id}, {"skill", project->skill},
                            {"markdown", read_skill(directory, *project)}};
            }});

        operations.add({
            "search_skills", "Find CyHub skills for import into a shared skill library.",
            {{"type", "object"}, {"additionalProperties", false},
             {"properties", {{"query", {{"type", "string"}, {"maxLength", 120}}},
                              {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 300}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext&) {
                auto items = Json::array();
                const auto query = args.value("query", "");
                const auto limit = args.value("limit", catalog.size());
                for (const auto& project : catalog) {
                    if (!project.skill.empty() && matches(project, query))
                        items.push_back({{"id", project.id}, {"name", project.skill}, {"title", project.name}});
                    if (items.size() == limit)
                        break;
                }
                return Json{{"items", items}};
            }});

        operations.add({
            "get_skill", "Read a full skill document returned by search_skills.",
            {{"type", "object"}, {"additionalProperties", false}, {"required", {"skill_id"}},
             {"properties", {{"skill_id", {{"type", "string"}, {"maxLength", 80}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext&) {
                const auto* project = find_project(catalog, args.at("skill_id").get<std::string>());
                if (!project || project->skill.empty())
                    throw cymcp::Error("NotFound", "No skill for this project ID");
                return Json{{"id", project->id}, {"name", project->skill}, {"title", project->name},
                            {"content", read_skill(directory, *project)}};
            }});

        cymcp::McpServer server(operations, "CyHub");
        server.serve_stdio(std::cin, std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CyHub MCP: " << error.what() << '\n';
        return 1;
    }
}

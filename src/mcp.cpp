#include "hub.hpp"

#include <cymcp/mcp.hpp>
#include <iostream>

int main() {
    using namespace cyhub;
    try {
        const auto directory = executable_directory();
        const auto catalog = load_catalog(directory / "catalog.json");
        const auto root = managed_root();
        cymcp::Registry operations(cymcp::Policy::allow_all());
        const Json output_schema = {{"type", "object"}};
        const auto describe = [&](const Project& project) {
            const auto installed = installed_version(root, project);
            return Json{{"id", project.id}, {"name", project.name}, {"category", project.category},
                        {"description", project.description}, {"repository", project.url()},
                        {"windowsManagedPackage", project.windows_x64.has_value()},
                        {"skill", project.skill.empty() ? Json(nullptr) : Json(project.skill)},
                        {"installedVersion", installed ? Json(installed->version) : Json(nullptr)}};
        };

        operations.add({
            "list_projects", "List CyHub projects and their managed installed versions.",
            {{"type", "object"}, {"additionalProperties", false},
             {"properties", {{"query", {{"type", "string"}, {"maxLength", 120}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext&) {
                auto projects = Json::array();
                for (const auto& project : catalog) {
                    if (matches(project, args.value("query", "")))
                        projects.push_back(describe(project));
                }
                return Json{{"projects", projects}};
            }});

        operations.add({
            "check_project_update", "Read the latest GitHub release and compare it with CyHub's installed version.",
            {{"type", "object"}, {"additionalProperties", false}, {"required", {"id"}},
             {"properties", {{"id", {{"type", "string"}, {"maxLength", 80}}}}}},
            output_schema, true,
            [&](const Json& args, const cymcp::CallContext& ctx) {
                const auto* project = find_project(catalog, args.at("id").get<std::string>());
                if (!project)
                    throw cymcp::Error("NotFound", "Unknown project ID");
                const auto release = latest_release(*project, ctx.cancellation);
                const auto installed = installed_version(root, *project);
                return Json{{"project", describe(*project)},
                            {"latestVersion", release ? Json(release->tag) : Json(nullptr)},
                            {"releaseUrl", release ? Json(release->url) : Json(nullptr)},
                            {"managedAssetAvailable", release && release->asset.has_value()},
                            {"updateAvailable", release && installed && newer_version(installed->version, release->tag)}};
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

        cymcp::McpServer server(operations, "CyHub");
        server.serve_stdio(std::cin, std::cout);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CyHub MCP: " << error.what() << '\n';
        return 1;
    }
}

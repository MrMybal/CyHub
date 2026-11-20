---
id: catalog
title: Catalog
description: Find projects by name, description, or category and add your own entries.
section: CyHub
parent: overview
order: 2
revision: 1
---

## Find a project {#search}

Choose **Tous les projets** or a category. Search by name, description or repository. The search matches accented words even if you type them without accents. Select a row for the project description, versions and GitHub link.

Choose **Skills MCP** to see projects with an integration guide. Each guide can be opened or copied from the project panel. See [MCP and skills](doc:mcp).

## Add a project {#customize}

Edit `catalog.json` in the package with the app closed. Use a stable lowercase `id`, a `name`, a `category` and a `description`. If available, set `repo` to the GitHub owner and repository separated by a slash. To associate an icon, place a PNG named after the project ID in `assets/icons`. To associate a guide, set `skill` to its folder name under `skills`.

For a managed Windows x64 package, use `windowsX64.asset` for the release asset name and `windowsX64.entry` for the executable path inside the ZIP or the local name of a portable EXE or wheel. The variable `{version}` expands from the release tag. Set `includePrerelease` to `true` if the project tracks prereleases.

To update the catalog used by the Windows service, run the installer again from the package with the modified catalog.

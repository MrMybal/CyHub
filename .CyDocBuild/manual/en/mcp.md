---
id: mcp
title: MCP and skills
description: Connect CyHub to an AI client and read a project guide.
section: CyHub
parent: overview
order: 4
revision: 3
---

## Connect an MCP client {#connect}

Set the client's stdio command to the absolute path of `CyHubMCP.exe` in your CyHub package. The server uses [CyMCPSDK](https://github.com/MrMybal/CyMCPSDK). Choose a client that supports MCP protocol 2025-11-25 or 2025-06-18.

| Tool | Result |
| --- | --- |
| `list_projects` | Projects, categories, managed installed versions and skill IDs. |
| `check_project_update` | Latest GitHub release and comparison with the version managed by CyHub. |
| `list_project_skills` | Project IDs with a bundled guide. |
| `get_project_skill` | The selected project's Markdown guide. |

Use an ID returned by the list methods to request an update check or a skill. The tools are read-only. CyHub also offers `search_skills` and `get_skill` for clients that import skills.

## Use a project skill {#skills}

Choose **MCP Skills**, select a project, and click **Read skill**. Copy the Markdown or open the corresponding `skills/<name>/SKILL.md` file from the package. Each guide describes how to find and connect to that project's own tools.

## Install and connect CyMCPHub {#companion}

[CyMCPHub](https://github.com/MrMybal/CyMCPHub) is the recommended companion for finding skills, MCP servers, CLI tools and SDKs in one place.

1. Choose **Install CyMCPHub** on the CyHub home page or in **MCP Skills**. The Windows x64 preview includes a verified package and installs it for the current account.
2. Choose **Open CyMCPHub** in CyHub. This starts CyMCPHub with the read-only **CyHub** connector.
3. In CyMCPHub, select the **CyHub** connector under **MCP servers**, then choose **Skills → Collect connector skills** to add the guides to its library.

Choose **Check on GitHub** to look for a newer CyMCPHub version. GitHub releases use a verified asset named `CyMCPHub-<version>-windows-x64.zip`. To check and install releases from the private repository, start CyHub with a `GH_TOKEN` that has read access to the repository. The included package can be installed offline.

If CyMCPHub is already running, quit it from its tray menu and open it again from CyHub to load the connector or use the new version. The CyMCPHub library and settings stay in its user profile.

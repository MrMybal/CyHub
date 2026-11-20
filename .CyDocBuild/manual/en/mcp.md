---
id: mcp
title: MCP and skills
description: Connect CyHub to an AI client and read a project guide.
section: CyHub
parent: overview
order: 4
revision: 2
---

## Connect an MCP client {#connect}

Set the client's stdio command to the absolute path of `CyHubMCP.exe` in your CyHub package. The server uses [CyMCPSDK](https://github.com/MrMybal/CyMCPSDK). Choose a client that supports MCP protocol 2025-11-25 or 2025-06-18.

| Tool | Result |
| --- | --- |
| `list_projects` | Projects, categories, managed installed versions and skill IDs. |
| `check_project_update` | Latest GitHub release and comparison with the version managed by CyHub. |
| `list_project_skills` | Project IDs with a bundled guide. |
| `get_project_skill` | The selected project's Markdown guide. |

Use an ID returned by the list methods to request an update check or a skill. The four tools are read-only.

## Use a project skill {#skills}

Choose **MCP Skills**, select a project, and click **Read skill**. Copy the Markdown or open the corresponding `skills/<name>/SKILL.md` file from the package. Each guide describes how to find and connect to that project's own tools.

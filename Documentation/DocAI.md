# CyHub agent guide

- Read [the project README](../README.md) for Windows build and packaging commands.
- [The catalog](../catalog.json) defines project IDs, asset names, entry points and skill folders. Keep project IDs stable because the updater uses them to locate managed versions.
- [Project skills](../skills/) describe connections to individual applications. [The CyHub MCP server](../src/mcp.cpp) exposes read-only tools to retrieve those skills, list projects and check releases.
- Change the [English and French sources](../.CyDocBuild/manual/) together. Keep page IDs and explicit anchors stable. Generate the reader with the [CyDoc build instructions](../.CyDocBuild/README.md).
- Open [the generated reader](../Documentation/Documentation.html#/en/overview) from disk and keep its `DocData` folder together with the HTML.

# CyHub 0.4.0

[Français](VERSION.fr.md) · [README](README.md)

**Status:** In development.

[Windows x64 preview](https://github.com/MrMybal/CyHub/releases)

## Included

- CyModelHub recommended in the toolbar and catalog, with a guide for sharing local AI models through MCP.
- Windows installer and verified GitHub self-updates, compact List and Cards modes, batch installs and richer project pages.
- CyHub logo in the desktop window, catalog and Windows executable icon, with the same artwork in both READMEs.
- Automatic Windows installation detection at startup, with an Installed apps view and GitHub update checks for known installed versions.
- Installed status, version and source in the MCP catalog.
- Recommended CyMCPHub card with direct offline installation of version 0.3.0, per-user updates and an MCP bridge for the bundled skills.

- Searchable CyUI catalog for applications, libraries, SDKs and extensions.
- Project artwork where available.
- English-first desktop interface with a saved English or French preference.
- GitHub release checks, verified package installation and a Windows x64 update service.
- CyMCPSDK stdio tools and bundled project skills.
- English and French offline CyDoc manual, available from Help or F1.

## Current limitations

- Managed installs cover selected Windows x64 packages and installations created by CyHub.
- Windows discovery covers registered apps, Start menu and desktop shortcuts, and common application folders. Portable apps stored elsewhere need a matching shortcut or registration to appear.
- Release checks follow GitHub API limits. Private repositories require an existing `GH_TOKEN` with release access. Externally installed apps link to the publisher's release for updates.
- The desktop code shares its SDL3 and MCP components across platforms. Linux and macOS builds, packaging and updater integration are pending.

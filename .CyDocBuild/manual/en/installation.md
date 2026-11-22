---
id: installation
title: Installation
description: Start the Windows desktop app and enable the update service.
section: CyHub
parent: overview
order: 1
revision: 7
---

## Launch CyHub {#launch}

1. Run `CyHub-Setup-windows-x64.exe` and select installation for your Windows account.
2. Launch **CyHub** from the Start menu.
3. Open **Settings** to choose your language, download folder and startup update checks.

The app uses the [Microsoft Visual C++ x64 runtime](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist). For portable use, extract `CyHub-windows-x64.zip` and open `CyHub.exe` from the extracted folder.

## Enable managed updates {#service}

Select installation for all users and keep the default Program Files location to enable the service. With the portable ZIP, run `Install-CyHub.ps1` from an administrator PowerShell x64 window.

The sidebar shows **Service running** when CyHub Updater is running. The update service uses the Windows LocalService account. It continues a requested update after the app window closes.

Run `Uninstall-CyHubService.ps1` as administrator to remove the service. The app, catalog and managed versions stay in Program Files.

## Update CyHub itself {#upgrade}

1. Click **Check CyHub** below the version in the sidebar.
2. When **Update CyHub** appears, click it to download the newer release's installer and check its size and SHA-256 against GitHub.
3. Follow the installer when CyHub closes. Select the same installation scope to update an existing installation.

If the release only provides a portable ZIP, **Update CyHub** opens that release. The installer preserves Windows user preferences and managed project versions. Merge custom catalog entries into the distribution before replacing its packaged catalog.

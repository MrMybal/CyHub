---
id: installation
title: Installation
description: Start the Windows desktop app and enable the update service.
section: CyHub
parent: overview
order: 1
revision: 1
---

## Launch CyHub {#launch}

1. Extract the complete Windows x64 package into a folder.
2. Open `CyHub.exe`. Keep the catalog, icons, skills and `Documentation` beside the executable.
3. Select a project and choose **Vérifier sur GitHub** to see its latest release.

The app uses the Microsoft Visual C++ x64 runtime. The portable catalog, skills and manual are available from the extracted folder.

## Enable managed updates {#service}

Close CyHub before installation. From a PowerShell x64 window running as administrator, run `Install-CyHub.ps1` from the extracted package. Then open `%ProgramFiles%\CyHub\CyHub.exe`.

The sidebar shows **Service actif** when CyHub Updater is running. The update service uses the Windows LocalService account. It continues a requested update after the app window closes.

Run `Uninstall-CyHubService.ps1` as administrator to remove the service. The app, catalog and managed versions stay in Program Files.

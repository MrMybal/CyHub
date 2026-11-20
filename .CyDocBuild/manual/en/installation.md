---
id: installation
title: Installation
description: Start the Windows desktop app and enable the update service.
section: CyHub
parent: overview
order: 1
revision: 6
---

## Launch CyHub {#launch}

1. Download `CyHub-windows-x64.zip` from the [GitHub preview](https://github.com/MrMybal/CyHub/releases/tag/v0.3.1) and extract it into a folder.
2. Open `CyHub.exe`. Keep the catalog, icons, skills and `Documentation` beside the executable.
3. Select a project and choose **Check on GitHub** to see its latest release.

The app uses the [Microsoft Visual C++ x64 runtime](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist). The portable catalog, skills and manual are available from the extracted folder.

## Enable managed updates {#service}

Close CyHub before installation. From a PowerShell x64 window running as administrator, run `Install-CyHub.ps1` from the extracted package. Then open `%ProgramFiles%\CyHub\CyHub.exe`.

The sidebar shows **Service running** when CyHub Updater is running. The update service uses the Windows LocalService account. It continues a requested update after the app window closes.

Run `Uninstall-CyHubService.ps1` as administrator to remove the service. The app, catalog and managed versions stay in Program Files.

## Update CyHub itself {#upgrade}

1. Close CyHub and extract the newer package into a fresh folder.
2. For portable use, open `CyHub.exe` from the new folder. Copy any project entries and icons you added to the new catalog and assets.
3. If you use CyHub Updater, run `Install-CyHub.ps1` as administrator from the new folder. Merge your custom catalog entries before running the script because it copies the catalog from the package.

The installer replaces the application and service files. It preserves managed project versions in `%ProgramFiles%\CyHub\Managed` and the saved language in your Windows profile.

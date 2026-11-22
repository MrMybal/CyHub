---
id: updates
title: Updates
description: Check GitHub releases and install verified Windows packages.
section: CyHub
parent: overview
order: 3
revision: 4
---

## Find installed apps {#detect}

CyHub scans Windows at startup, even when CyHub Updater is stopped. It checks Windows app registrations, Start menu and desktop shortcuts, and common locations such as Program Files, the user's Programs folder, Desktop and Downloads. The **Installed apps** view shows matching catalog projects. Select a project to see its version and how CyHub found it.

Use **Scan again** after installing or removing an app. For a portable app stored in another folder, put a shortcut to its executable in the Start menu or on the desktop. An app can appear with **Version unknown** if Windows provides no usable version.

## Check releases {#check}

CyHub checks accessible GitHub releases for detected projects with a known version. Set the startup check in **Settings**. **Updates** shows installed and available versions, the publication date and a red count when updates are available. Use **Check for updates** to repeat the comparison, or **Check on GitHub** for one project. If a project was installed outside CyHub, use **View release** and follow the publisher's update instructions.

Release checks follow GitHub API limits. To check private repositories, launch CyHub with an existing `GH_TOKEN` that has read access to the releases.

## Install or update {#install}

1. Select a project, check its release, then click **Install** or **Update**.
2. To queue several compatible applications, tick their checkboxes and choose **Install selected**.
3. Use **Open application** when the installation is ready. A managed library wheel offers **Open folder**.

CyHub verifies the published SHA-256 before activating a version. Per-user packages are installed under `%LocalAppData%\CyHub\Projects`. [CyHub Updater](doc:installation#service) can manage shared packages in `%ProgramFiles%\CyHub\Managed`. In **Settings**, pick a download folder for CyHub installers and plug-in ZIPs.

---
id: updates
title: Updates
description: Check GitHub releases and install verified Windows packages.
section: CyHub
parent: overview
order: 3
revision: 1
---

## Check releases {#check}

Choose **Vérifier sur GitHub** for a selected project or **Tout vérifier** for the catalog. **Mises à jour** lists managed installations with a newer release. Projects without a matching package still show the release link when one exists. Release checks use the public GitHub API and are subject to its rate limit.

## Install or update {#install}

1. Enable [CyHub Updater](doc:installation#service).
2. Select a project, check its release, then click **Installer** or **Mettre à jour**.
3. Let the service download and verify the package. Use **Ouvrir l’application** when the installation is ready. A managed library wheel offers **Ouvrir le dossier**.

CyHub verifies the published SHA-256 and prepares the next version before making it active. Existing managed versions stay in `%ProgramFiles%\CyHub\Managed`. Applications open under your Windows account. CyHub manages only the installations it creates.

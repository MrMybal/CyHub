---
id: installation
title: Installation
description: Démarrer l’application Windows et activer le service de mise à jour.
section: CyHub
parent: overview
order: 1
sourceRevision: 1
---

## Ouvrir CyHub {#launch}

1. Extrais le paquet Windows x64 complet dans un dossier.
2. Ouvre `CyHub.exe`. Garde le catalogue, les icônes, les skills et le dossier `Documentation` à côté de l’exécutable.
3. Sélectionne un projet, puis clique sur **Vérifier sur GitHub** pour voir sa dernière publication.

L’application utilise le runtime Microsoft Visual C++ x64. Le catalogue portable, les skills et le manuel sont disponibles dans le dossier extrait.

## Activer les mises à jour gérées {#service}

Ferme CyHub avant l’installation. Ouvre PowerShell x64 en tant qu’administrateur et lance `Install-CyHub.ps1` depuis le paquet extrait. Ouvre ensuite `%ProgramFiles%\CyHub\CyHub.exe`.

La barre latérale affiche **Service actif** quand CyHub Updater tourne. Le service utilise le compte Windows LocalService et poursuit une mise à jour demandée après la fermeture de la fenêtre.

Lance `Uninstall-CyHubService.ps1` en administrateur pour retirer le service. L’application, le catalogue et les versions gérées restent dans Program Files.

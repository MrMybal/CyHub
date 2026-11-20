---
id: installation
title: Installation
description: Démarrer l’application Windows et activer le service de mise à jour.
section: CyHub
parent: overview
order: 1
sourceRevision: 4
---

## Ouvrir CyHub {#launch}

1. Télécharge `CyHub-windows-x64.zip` depuis la [préversion GitHub](https://github.com/MrMybal/CyHub/releases/tag/v0.2.0) et extrais-le dans un dossier.
2. Ouvre `CyHub.exe`. Garde le catalogue, les icônes, les skills et le dossier `Documentation` à côté de l’exécutable.
3. Choisis **Language → Français** si tu souhaites passer l’interface en français. Sélectionne un projet, puis clique sur **Vérifier sur GitHub** pour voir sa dernière publication.

L’application utilise le [runtime Microsoft Visual C++ x64](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist). Le catalogue portable, les skills et le manuel sont disponibles dans le dossier extrait.

## Activer les mises à jour gérées {#service}

Ferme CyHub avant l’installation. Ouvre PowerShell x64 en tant qu’administrateur et lance `Install-CyHub.ps1` depuis le paquet extrait. Ouvre ensuite `%ProgramFiles%\CyHub\CyHub.exe`.

La barre latérale affiche **Service actif** quand CyHub Updater tourne. Le service utilise le compte Windows LocalService et poursuit une mise à jour demandée après la fermeture de la fenêtre.

Lance `Uninstall-CyHubService.ps1` en administrateur pour retirer le service. L’application, le catalogue et les versions gérées restent dans Program Files.

## Mettre à jour CyHub lui-même {#upgrade}

1. Ferme CyHub et extrais le nouveau paquet dans un dossier vide.
2. En mode portable, ouvre `CyHub.exe` depuis le nouveau dossier. Reporte les projets et les icônes que tu avais ajoutés dans le nouveau catalogue et les ressources.
3. Si tu utilises CyHub Updater, exécute `Install-CyHub.ps1` en administrateur depuis le nouveau dossier. Fusionne tes entrées personnalisées avant de lancer le script, qui récupère le catalogue du paquet.

Le script remplace les fichiers de l’application et du service. Les versions gérées restent dans `%ProgramFiles%\CyHub\Managed` et la langue mémorisée reste dans ton profil Windows.

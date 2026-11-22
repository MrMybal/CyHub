---
id: installation
title: Installation
description: Démarrer l’application Windows et activer le service de mise à jour.
section: CyHub
parent: overview
order: 1
sourceRevision: 7
---

## Ouvrir CyHub {#launch}

1. Lance `CyHub-Setup-windows-x64.exe` et choisis l’installation pour ton compte Windows.
2. Ouvre **CyHub** depuis le menu Démarrer.
3. Ouvre **Settings → Language → Français** pour changer de langue. Les **Paramètres** permettent aussi de choisir le dossier de téléchargement et la vérification au démarrage.

L’application utilise le [runtime Microsoft Visual C++ x64](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist). Pour travailler en mode portable, extrais `CyHub-windows-x64.zip` et ouvre `CyHub.exe` depuis le dossier extrait.

## Activer les mises à jour gérées {#service}

Choisis l’installation pour tous les utilisateurs et garde le dossier Program Files par défaut pour activer le service. Avec le ZIP portable, exécute `Install-CyHub.ps1` dans une fenêtre PowerShell x64 ouverte en administrateur.

La barre latérale affiche **Service actif** quand CyHub Updater tourne. Le service utilise le compte Windows LocalService et poursuit une mise à jour demandée après la fermeture de la fenêtre.

Lance `Uninstall-CyHubService.ps1` en administrateur pour retirer le service. L’application, le catalogue et les versions gérées restent dans Program Files.

## Mettre à jour CyHub lui-même {#upgrade}

1. Clique sur **Vérifier CyHub** sous la version dans la barre latérale.
2. Quand **Mettre à jour CyHub** apparaît, clique dessus pour télécharger l’installateur et vérifier sa taille et son SHA-256 auprès de GitHub.
3. Suis les étapes de l’installateur après la fermeture de CyHub. Garde le même type d’installation pour mettre à jour une copie existante.

Si la publication fournit seulement un ZIP portable, **Mettre à jour CyHub** ouvre la publication. L’installateur conserve tes paramètres Windows et les versions gérées de tes projets. Fusionne les entrées personnalisées avant de remplacer le catalogue fourni.

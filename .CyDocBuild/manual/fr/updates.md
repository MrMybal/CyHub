---
id: updates
title: Mises à jour
description: Consulter les publications GitHub et installer les paquets Windows vérifiés.
section: CyHub
parent: overview
order: 3
sourceRevision: 3
---

## Trouver les applications installées {#detect}

CyHub analyse Windows au démarrage, même si le service CyHub Updater est arrêté. Il consulte les applications enregistrées, les raccourcis du menu Démarrer et du Bureau, ainsi que les dossiers courants comme Program Files, le dossier Programs de ton compte, le Bureau et Téléchargements. La vue **Applications installées** affiche les projets du catalogue retrouvés. Sélectionne un projet pour voir sa version et sa source de détection.

Utilise **Relancer la détection** après avoir installé ou supprimé une application. Pour une application portable rangée dans un autre dossier, crée un raccourci vers son exécutable dans le menu Démarrer ou sur le Bureau. L’application peut apparaître avec la mention **Version inconnue** si Windows ne fournit aucune version exploitable.

## Vérifier les versions {#check}

CyHub vérifie les publications GitHub accessibles des projets dont il connaît la version installée. Ouvre **Mises à jour** pour voir les versions plus récentes ou clique sur **Vérifier les mises à jour** pour relancer la détection et la comparaison. **Vérifier sur GitHub** porte sur le projet sélectionné, tandis que **Tout vérifier** couvre l’ensemble du catalogue. Pour un projet installé en dehors de CyHub, clique sur **Voir la publication** et suis les instructions de l’éditeur.

Les vérifications respectent les limites de l’API GitHub. Pour consulter les publications des dépôts privés, lance CyHub avec un `GH_TOKEN` existant qui donne accès à ces publications.

## Installer ou mettre à jour {#install}

1. Active [CyHub Updater](doc:installation#service).
2. Sélectionne un projet, vérifie sa publication, puis clique sur **Installer** ou **Mettre à jour**.
3. Attends que le service télécharge et vérifie le paquet. Clique sur **Ouvrir l’application** une fois l’installation prête. Pour un wheel géré, utilise **Ouvrir le dossier**.

CyHub vérifie le SHA-256 publié et prépare la nouvelle version avant de l’activer. Les versions gérées précédentes restent dans `%ProgramFiles%\CyHub\Managed`. Les applications s’ouvrent dans ta session Windows. CyHub gère les installations qu’il crée.

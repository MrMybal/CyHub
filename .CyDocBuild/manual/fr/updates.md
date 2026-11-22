---
id: updates
title: Mises à jour
description: Consulter les publications GitHub et installer les paquets Windows vérifiés.
section: CyHub
parent: overview
order: 3
sourceRevision: 4
---

## Trouver les applications installées {#detect}

CyHub analyse Windows au démarrage, même si le service CyHub Updater est arrêté. Il consulte les applications enregistrées, les raccourcis du menu Démarrer et du Bureau, ainsi que les dossiers courants comme Program Files, le dossier Programs de ton compte, le Bureau et Téléchargements. La vue **Applications installées** affiche les projets du catalogue retrouvés. Sélectionne un projet pour voir sa version et sa source de détection.

Utilise **Relancer la détection** après avoir installé ou supprimé une application. Pour une application portable rangée dans un autre dossier, crée un raccourci vers son exécutable dans le menu Démarrer ou sur le Bureau. L’application peut apparaître avec la mention **Version inconnue** si Windows ne fournit aucune version exploitable.

## Vérifier les versions {#check}

CyHub vérifie les publications GitHub accessibles des projets dont il connaît la version installée. Le contrôle au démarrage se règle dans **Paramètres**. **Mises à jour** affiche les versions installées et disponibles, la date de publication et un compteur rouge quand des mises à jour sont disponibles. Clique sur **Vérifier les mises à jour** pour relancer la comparaison ou sur **Vérifier sur GitHub** pour un seul projet. Pour une application installée en dehors de CyHub, utilise **Voir la publication** et suis les instructions de l’éditeur.

Les vérifications respectent les limites de l’API GitHub. Pour consulter les publications des dépôts privés, lance CyHub avec un `GH_TOKEN` existant qui donne accès à ces publications.

## Installer ou mettre à jour {#install}

1. Sélectionne un projet, vérifie sa publication, puis clique sur **Installer** ou **Mettre à jour**.
2. Pour installer plusieurs applications compatibles, coche-les puis clique sur **Installer la sélection**.
3. Clique sur **Ouvrir l’application** une fois l’installation prête. Pour un wheel géré, utilise **Ouvrir le dossier**.

CyHub vérifie le SHA-256 publié avant d’activer la version. Les paquets par compte sont installés dans `%LocalAppData%\CyHub\Projects`. [CyHub Updater](doc:installation#service) peut gérer les paquets partagés dans `%ProgramFiles%\CyHub\Managed`. Dans **Paramètres**, choisis le dossier de téléchargement des installateurs CyHub et des ZIP de plugins.

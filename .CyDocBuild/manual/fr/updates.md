---
id: updates
title: Mises à jour
description: Consulter les publications GitHub et installer les paquets Windows vérifiés.
section: CyHub
parent: overview
order: 3
sourceRevision: 2
---

## Vérifier les versions {#check}

Choisis **Vérifier sur GitHub** pour le projet sélectionné ou **Tout vérifier** pour le catalogue. **Mises à jour** affiche les installations gérées pour lesquelles une version plus récente est disponible. Lorsqu’un projet a une publication sans paquet compatible, le lien GitHub permet de la consulter. Les vérifications utilisent l’API publique GitHub et suivent sa limite de requêtes.

## Installer ou mettre à jour {#install}

1. Active [CyHub Updater](doc:installation#service).
2. Sélectionne un projet, vérifie sa publication, puis clique sur **Installer** ou **Mettre à jour**.
3. Attends que le service télécharge et vérifie le paquet. Clique sur **Ouvrir l’application** une fois l’installation prête. Pour un wheel géré, utilise **Ouvrir le dossier**.

CyHub vérifie le SHA-256 publié et prépare la nouvelle version avant de l’activer. Les versions gérées précédentes restent dans `%ProgramFiles%\CyHub\Managed`. Les applications s’ouvrent dans ta session Windows. CyHub gère les installations qu’il crée.

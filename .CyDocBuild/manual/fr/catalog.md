---
id: catalog
title: Catalogue
description: Rechercher par nom, description ou catégorie et ajouter des projets.
section: CyHub
parent: overview
order: 2
sourceRevision: 1
---

## Trouver un projet {#search}

Sélectionne **Tous les projets** ou une catégorie. Recherche un nom, une description ou un dépôt. Tu peux saisir des mots accentués sans leurs accents. Ouvre la fiche d’un projet pour lire sa description, comparer ses versions et accéder à GitHub.

Choisis **Skills MCP** pour afficher les projets qui proposent un guide d’intégration. La fiche du projet permet de lire et de copier son guide. Voir [MCP et skills](doc:mcp).

## Ajouter un projet {#customize}

Ferme l’application et modifie `catalog.json` dans le paquet. Renseigne un `id` stable en minuscules, un `name`, une `category` et une `description`. Pour GitHub, le champ `repo` prend la forme `propriétaire/dépôt`. Pour associer une icône, place un PNG nommé d’après l’identifiant du projet dans `assets/icons`. Pour associer un guide, renseigne `skill` avec son nom de dossier sous `skills`.

Pour un paquet Windows x64 géré, `windowsX64.asset` donne le nom du fichier publié. `windowsX64.entry` désigne le chemin de l’exécutable dans le ZIP ou le nom local de l’EXE portable ou du wheel. `{version}` est remplacé par la version tirée du tag. Active `includePrerelease` pour suivre les préversions.

Pour actualiser le catalogue utilisé par le service Windows, relance le script d’installation depuis le paquet contenant le catalogue modifié.

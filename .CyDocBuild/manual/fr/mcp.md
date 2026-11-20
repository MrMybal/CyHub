---
id: mcp
title: MCP et skills
description: Connecter un client IA à CyHub et lire les guides des projets.
section: CyHub
parent: overview
order: 4
sourceRevision: 3
---

## Connecter un client MCP {#connect}

Configure le chemin absolu de `CyHubMCP.exe` comme commande stdio dans ton client. Le serveur utilise [CyMCPSDK](https://github.com/MrMybal/CyMCPSDK). Choisis un client compatible avec le protocole MCP 2025-11-25 ou 2025-06-18.

| Outil | Résultat |
| --- | --- |
| `list_projects` | Projets, catégories, versions gérées installées et identifiants des skills. |
| `check_project_update` | Dernière publication GitHub et comparaison avec la version gérée par CyHub. |
| `list_project_skills` | Identifiants des projets ayant un guide. |
| `get_project_skill` | Guide Markdown du projet sélectionné. |

Utilise les identifiants renvoyés par les outils de liste pour vérifier une version ou lire un skill. Les outils sont en lecture seule. CyHub expose aussi `search_skills` et `get_skill` aux clients qui importent des skills.

## Utiliser le skill d’un projet {#skills}

Choisis **Skills MCP**, sélectionne un projet et clique sur **Lire le skill**. Copie le Markdown ou ouvre le fichier `skills/<nom>/SKILL.md` correspondant dans le paquet. Chaque guide indique comment trouver les outils propres au projet et s’y connecter.

## Installer et connecter CyMCPHub {#companion}

[CyMCPHub](https://github.com/MrMybal/CyMCPHub) est l’application recommandée pour retrouver tes skills, serveurs MCP, outils CLI et SDK au même endroit.

1. Clique sur **Installer CyMCPHub** dans l’accueil de CyHub ou dans **Skills MCP**. La préversion Windows x64 fournit un paquet vérifié qui s’installe pour le compte en cours.
2. Clique sur **Ouvrir CyMCPHub** dans CyHub. CyMCPHub démarre avec le connecteur **CyHub** en lecture seule.
3. Dans CyMCPHub, sélectionne le connecteur **CyHub** dans **Serveurs MCP**, puis choisis **Skills → Collecter les skills du connecteur** pour ajouter les guides à la bibliothèque.

Clique sur **Vérifier sur GitHub** pour rechercher une version plus récente de CyMCPHub. Les publications GitHub utilisent une archive vérifiée nommée `CyMCPHub-<version>-windows-x64.zip`. Pour vérifier et installer les publications du dépôt privé, lance CyHub avec une variable `GH_TOKEN` qui donne accès en lecture au dépôt. Le paquet inclus peut être installé hors ligne.

Si CyMCPHub est déjà ouvert, quitte-le depuis la zone de notification et rouvre-le depuis CyHub pour charger le connecteur ou utiliser la nouvelle version. La bibliothèque et les paramètres restent dans le profil utilisateur de CyMCPHub.

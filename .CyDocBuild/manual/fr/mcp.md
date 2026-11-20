---
id: mcp
title: MCP et skills
description: Connecter un client IA à CyHub et lire les guides des projets.
section: CyHub
parent: overview
order: 4
sourceRevision: 1
---

## Connecter un client MCP {#connect}

Configure le chemin absolu de `CyHubMCP.exe` comme commande stdio dans ton client. Le serveur utilise [CyMCPSDK](https://github.com/MrMybal/CyMCPSDK). Choisis un client compatible avec le protocole MCP 2025-11-25 ou 2025-06-18.

| Outil | Résultat |
| --- | --- |
| `list_projects` | Projets, catégories, versions gérées installées et identifiants des skills. |
| `check_project_update` | Dernière publication GitHub et comparaison avec la version gérée par CyHub. |
| `list_project_skills` | Identifiants des projets ayant un guide. |
| `get_project_skill` | Guide Markdown du projet sélectionné. |

Utilise les identifiants renvoyés par les outils de liste pour vérifier une version ou lire un skill. Les quatre outils sont en lecture seule.

## Utiliser le skill d’un projet {#skills}

Choisis **Skills MCP**, sélectionne un projet et clique sur **Lire le skill**. Copie le Markdown ou ouvre le fichier `skills/<nom>/SKILL.md` correspondant dans le paquet. Chaque guide indique comment trouver les outils propres au projet et s’y connecter.

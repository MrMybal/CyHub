---
name: cyhub
description: Explorer le catalogue CyHub, lire les skills MCP et vérifier les versions GitHub avec CyHubMCP.
---

# CyHub

## Connexion

Connecte le serveur stdio avec le chemin absolu de `CyHubMCP.exe` dans le dossier extrait. `list_projects` renvoie les identifiants à passer à `check_project_update` et `get_project_skill`. `list_project_skills` filtre les projets dont CyHub fournit un guide.

## Utilisation

Lis la version installée et la publication avant de conclure qu’une mise à jour est disponible. Une release peut exister sans paquet Windows géré. Les installations s’effectuent depuis l’application de bureau.

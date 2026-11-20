---
name: cydsize
description: Consulter les analyses d’espace disque, rapports et jobs de CyDSize via la CLI ou MCP.
---

# CyDSize

## Connexion

Utilise MCP / SDK → Start desktop MCP pour piloter la fenêtre ouverte. `cydsize-cli.exe mcp` expose les scans indépendants et les rapports. La CLI hérite des droits de son processus.

## Utilisation

Identifie le volume, le dossier et le statut du scan. Vérifie que le rapport est terminé, puis distingue les tailles logiques des tailles mesurées sur disque.

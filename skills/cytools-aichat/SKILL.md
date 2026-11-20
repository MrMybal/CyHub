---
name: cytools-aichat
description: Utiliser CyTools_AIChat avec son MCP natif pour gérer les sessions de conversation et leurs exports.
---

# CyTools_AIChat

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Identifie la conversation et le fournisseur. Si un fournisseur CLI est en lecture seule, son état l’indique dans la configuration de l’application. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_AIChat](https://github.com/MrMybal/CyTools_AIChat/blob/main/README.md#native-ai-communication).

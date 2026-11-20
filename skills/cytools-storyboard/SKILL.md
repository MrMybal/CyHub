---
name: cytools-storyboard
description: Utiliser CyTools_Storyboard avec son MCP natif pour préparer des storyboards, plans et exports.
---

# CyTools_Storyboard

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Identifie le plan par son ID dans la séquence. Vérifie les images sources et la destination avant de lancer un export ou un rendu. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_Storyboard](https://github.com/MrMybal/CyTools_Storyboard/blob/main/README.md#native-ai-communication).

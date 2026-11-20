---
name: cytools-audiolibrary
description: Utiliser CyTools_AudioLibrary avec son MCP natif pour parcourir une collection audio et identifier ses éléments.
---

# CyTools_AudioLibrary

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Borne les requêtes à la collection, dossier ou liste concernée. Avant une opération qui écrit des métadonnées, relève l’élément et son chemin source. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_AudioLibrary](https://github.com/MrMybal/CyTools_AudioLibrary/blob/main/README.md#native-ai-communication).

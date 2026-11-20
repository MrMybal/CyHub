---
name: cytools-audiostudio
description: Utiliser CyTools_AudioStudio avec son MCP natif pour préparer des traitements audio et retrouver les résultats.
---

# CyTools_AudioStudio

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Vérifie la source, le format et la destination avant le traitement. Attends le job et inspecte le fichier audio produit. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_AudioStudio](https://github.com/MrMybal/CyTools_AudioStudio/blob/main/README.md#native-ai-communication).

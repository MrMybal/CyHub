---
name: cytools-subtitle
description: Utiliser CyTools_Subtitle avec son MCP natif pour transcrire, corriger et exporter des sous-titres.
---

# CyTools_Subtitle

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Vérifie le moteur de transcription et la piste sélectionnée. La séparation des locuteurs nécessite son moteur propre. Inspecte puis exporte en SRT, WebVTT ou JSON selon la demande. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

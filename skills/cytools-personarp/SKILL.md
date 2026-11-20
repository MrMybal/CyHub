---
name: cytools-personarp
description: Utiliser CyTools_PersonaRP avec son MCP natif pour retrouver les personnages, sessions et paramètres de génération.
---

# CyTools_PersonaRP

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Vérifie le personnage et la session active avant de modifier les paramètres de génération. Exporter une conversation et modifier un profil sont deux opérations distinctes. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_PersonaRP](https://github.com/MrMybal/CyTools_PersonaRP/blob/main/README.md#native-ai-communication).

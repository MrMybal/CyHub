---
name: cytools-meshstudio
description: Utiliser CyTools_MeshStudio avec son MCP natif pour travailler sur des maillages, rigs et étapes de génération 3D.
---

# CyTools_MeshStudio

## Connexion

Le paquet fournit le runtime natif CyToolsCore et son pont CyMCPSDK. Utilise la configuration CLI/MCP de l’application. Interroge `ai_communication_status` avec `includeOperations: true` sur une version compatible pour connaître les modes et opérations de cette installation.

## Utilisation

Vérifie la disponibilité des backends optionnels pour le rig, le mouvement et le raffinement. Le catalogue documente aussi les étapes qui ne sont pas disponibles dans l’installation active. Suis le job jusqu’à un résultat terminal puis exporte les artefacts utiles avant le nettoyage.

## Documentation

[Guide de CyTools_MeshStudio](https://github.com/MrMybal/CyTools_MeshStudio/blob/main/README.md#native-ai-communication).

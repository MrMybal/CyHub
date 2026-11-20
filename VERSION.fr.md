# CyHub 0.1.0

[English](VERSION.md) · [README](README.fr.md)

**Statut** en développement.

[Préversion Windows x64](https://github.com/MrMybal/CyHub/releases/tag/v0.1.0)

## Inclus

- Catalogue CyUI avec recherche pour les applications, bibliothèques, SDK et extensions.
- Icônes des projets lorsqu’elles sont disponibles.
- Interface de bureau en anglais par défaut, avec préférence de langue anglais ou français mémorisée.
- Vérification des publications GitHub, installation de paquets vérifiés et service de mise à jour Windows x64.
- Outils stdio via CyMCPSDK et skills fournis pour les projets.
- Manuel CyDoc hors ligne en français et en anglais, accessible par **Aide** ou **F1**.

## Limites actuelles

- L’installation gérée couvre certains paquets Windows x64 et les installations créées par CyHub.
- La vérification utilise l’API publique GitHub et suit sa limite de requêtes. Les projets dont le format de paquet diffère renvoient vers leur publication.
- Le bureau partage ses composants SDL3 et MCP entre plateformes. Les builds, les paquets et l’intégration de la mise à jour restent à valider sous Linux et macOS.

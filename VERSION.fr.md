# CyHub 0.3.0

[English](VERSION.md) · [README](README.fr.md)

**Statut** en développement.

[Préversion Windows x64](https://github.com/MrMybal/CyHub/releases/tag/v0.3.0)

## Inclus

- Détection automatique des installations Windows au démarrage, avec la vue Applications installées et la recherche de mises à jour GitHub pour les versions connues.
- État, version et source des installations dans le catalogue MCP.
- Carte CyMCPHub recommandée avec installation hors ligne de la version 0.3.0, mises à jour par compte et pont MCP pour les skills inclus.

- Catalogue CyUI avec recherche pour les applications, bibliothèques, SDK et extensions.
- Icônes des projets lorsqu’elles sont disponibles.
- Interface de bureau en anglais par défaut, avec préférence de langue anglais ou français mémorisée.
- Vérification des publications GitHub, installation de paquets vérifiés et service de mise à jour Windows x64.
- Outils stdio via CyMCPSDK et skills fournis pour les projets.
- Manuel CyDoc hors ligne en français et en anglais, accessible par **Aide** ou **F1**.

## Limites actuelles

- L’installation gérée couvre certains paquets Windows x64 et les installations créées par CyHub.
- La détection Windows couvre les applications enregistrées, les raccourcis du menu Démarrer et du Bureau, ainsi que les dossiers d’installation courants. Les applications portables rangées ailleurs apparaissent si elles disposent d’un raccourci ou d’une entrée correspondante.
- La vérification respecte les limites de l’API GitHub. Les dépôts privés nécessitent un `GH_TOKEN` existant qui donne accès à leurs publications. Pour une application installée en dehors de CyHub, la mise à jour passe par la publication de l’éditeur.
- Le bureau partage ses composants SDL3 et MCP entre plateformes. Les builds, les paquets et l’intégration de la mise à jour restent à valider sous Linux et macOS.

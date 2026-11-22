---
name: cymodelhub
description: Retrouver, vérifier et partager des modèles IA locaux entre applications compatibles via CyModelHub et ses outils MCP.
---

# CyModelHub

## Connexion

Lance CyModelHub et utilise **Réglages → Copier la configuration MCP** pour obtenir la configuration HTTP locale. Pour une connexion stdio, configure le client pour lancer `CyModelHubMcp` depuis le dossier d’installation. Garde le jeton HTTP dans la configuration locale du client.

## Réutilisation d’un modèle

- Appelle `models_list` pour inventorier les modèles et `models_resolve` pour obtenir le chemin d’un modèle déjà présent. Utilise `models_check` quand une application a besoin de vérifier qu’il est prêt.
- Utilise le chemin retourné avec les applications qui acceptent un chemin de modèle. Si une application impose son propre dossier, `models_link` permet d’y rendre le modèle accessible. Un lien physique nécessite le même volume.
- Utilise `roots_add` pour surveiller un dossier existant ou `models_register` pour référencer un modèle complet déjà sur disque. Si un téléchargement est demandé, `install_start` réutilise un modèle déjà installé ou rejoint son installation en cours. Suis le résultat via `jobs_get`.

Consulte la [référence des outils](https://github.com/MrMybal/CyModelHub/blob/main/docs/API.md) pour les arguments de chaque opération.

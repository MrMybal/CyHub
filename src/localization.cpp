#include "localization.hpp"

#include <cyui/Core.h>
#include <utility>

namespace cyhub {
namespace {
constexpr std::pair<std::string_view, std::string_view> diagnostics[] = {
    {"The bundled CyMCPHub package manifest is invalid.", "Le manifeste du paquet CyMCPHub inclus est invalide."},
    {"Could not access the CyMCPHub installation directory.", "Impossible d’accéder au dossier d’installation de CyMCPHub."},
    {"The authenticated GitHub asset is unavailable.", "Le paquet GitHub authentifié est indisponible."},
    {"Could not prepare the application environment.", "Impossible de préparer l’environnement de l’application."},
    {"Could not locate the local user data directory.", "Impossible de trouver le dossier de données utilisateur local."},
    {"Unable to read file: ", "Fichier illisible : "},
    {"The file is too large or unreadable.", "Fichier trop volumineux ou illisible."},
    {"CyHub has no skill for this project.", "Ce projet n’a aucun skill dans CyHub."},
    {"The catalog must contain between 1 and 300 projects.", "Le catalogue doit contenir entre 1 et 300 projets."},
    {"Invalid project or duplicate ID: ", "Projet invalide ou identifiant en double : "},
    {"Invalid skill ID: ", "Identifiant de skill invalide : "},
    {"Invalid GitHub repository: ", "Dépôt GitHub invalide : "},
    {"Invalid Windows package: ", "Paquet Windows invalide : "},
    {"Could not initialize the renderer.", "Initialisation du moteur d’affichage impossible."},
    {"Could not write the update state.", "Écriture de l’état impossible."},
    {"Publishing the update state", "Publication de l’état"},
    {"The ZIP file is unreadable.", "Le fichier ZIP est illisible."},
    {"The ZIP contains an unsupported number of entries.", "Nombre de fichiers ZIP hors limites."},
    {"The ZIP exceeds the unpacked size limit.", "Taille décompressée du ZIP hors limites."},
    {"Invalid ZIP entry name.", "Nom de fichier ZIP invalide."},
    {"The ZIP contains an unsupported entry.", "Le ZIP contient une entrée non prise en charge."},
    {"The ZIP contains duplicate file names.", "Le ZIP contient plusieurs fichiers portant le même nom."},
    {"Could not extract a file from the ZIP.", "Impossible d’extraire un fichier du ZIP."},
    {"Could not save a file from the ZIP.", "Impossible d’enregistrer un fichier du ZIP."},
    {"The Windows package cannot be verified.", "Paquet Windows non vérifiable."},
    {"The package size differs from the GitHub release.", "Taille du paquet différente de la publication GitHub."},
    {"The SHA-256 checksum does not match the GitHub release.", "L’empreinte SHA-256 ne correspond pas à la publication GitHub."},
    {"Invalid package entry point.", "Point d’entrée du paquet invalide."},
    {"The main file is missing from the package.", "Le fichier principal est absent du paquet."},
    {"The version directory is incomplete.", "Le dossier de version est incomplet."},
    {"The requested package changed or is unavailable.", "Le paquet demandé a changé ou n’est plus disponible."},
    {"This version is older than the active installation.", "Cette version est antérieure à l’installation active."},
    {"Could not create the download file.", "Impossible de créer le fichier de téléchargement."},
    {"Download incomplete or refused (HTTP ", "Téléchargement incomplet ou refusé (HTTP "},
    {"GitHub rate limit reached. Try again later.", "GitHub limite les requêtes. Réessaye plus tard."},
    {"GitHub returned HTTP ", "GitHub a répondu HTTP "},
    {"Download redirect refused.", "Redirection de téléchargement refusée."},
    {"Too many download redirects.", "Trop de redirections de téléchargement."},
    {"The update was interrupted. You can try again.", "Mise à jour interrompue. Tu peux la relancer."},
    {"Invalid request.", "Demande invalide."},
    {"The service catalog has no matching package.", "Aucun paquet correspondant dans le catalogue du service."},
    {"An installation is already in progress.", "Une installation est déjà en cours."},
    {"Could not locate CyHub.", "Chemin de CyHub introuvable."},
    {"Could not locate Program Files.", "Le dossier Program Files est introuvable."},
    {"Unknown installation.", "Installation inconnue."},
    {"Invalid entry point.", "Point d’entrée invalide."},
    {"Could not launch the application", "Démarrage impossible"},
    {"Could not open the folder.", "Impossible d’ouvrir le dossier."},
    {"Managed installation is available on Windows x64.", "L’installation gérée est prévue pour Windows x64."},
    {"The update service is available on Windows x64.", "Le service de mise à jour est disponible sous Windows x64."},
    {"CyHub Updater is stopped or missing.", "Le service CyHub Updater est arrêté ou absent."},
    {"The service is temporarily unavailable.", "Le service est temporairement indisponible."},
    {"Could not verify the service identity.", "Impossible de vérifier l’identité du service."},
    {"Could not send the request.", "La demande n’a pas pu être envoyée."},
    {"The service did not respond.", "Le service n’a pas répondu."},
    {"The update was refused.", "Mise à jour refusée."},
    {"Connecting to the service", "Connexion au service"},
    {"Creating the service pipe", "Création du canal du service"},
    {"Setting service permissions", "Autorisations du service"},
    {"Creating the service event", "Événement du service"},
    {"The preferences directory is unavailable.", "Dossier des préférences indisponible."},
    {"Could not save the language preference.", "Impossible d’enregistrer la langue choisie."},
    {"HTTP request failed", "Échec de la requête HTTP"},
};

bool diagnostic_prefix(std::string_view message, std::string_view prefix) {
    return message.starts_with(prefix) &&
        (message.size() == prefix.size() || prefix.ends_with(' ') ||
         message[prefix.size()] == ':' || message[prefix.size()] == ' ');
}
}

void register_ui_translations() {
    cyui::registerTranslations("fr", {
        {"Catalog", "Catalogue"},
        {"All projects", "Tous les projets"},
        {"Updates", "Mises à jour"},
        {"MCP Skills", "Skills MCP"},
        {"By category", "Par type"},
        {"Language", "Langue"},
        {"Help", "Aide"},
        {"●  Service running", "●  Service actif"},
        {"○  Service inactive", "○  Service inactif"},
        {"Catalog · GitHub", "Catalogue · GitHub"},
        {"The catalog is available in portable mode.", "Le catalogue reste accessible en mode portable."},
        {"Managed installation is available on Windows x64.", "L’installation gérée est disponible sous Windows x64."},
        {"Your collection", "Ta collection"},
        {"Connection guides for AI agents.", "Guides de connexion pour les agents IA."},
        {"Applications, libraries and extensions.", "Applications, bibliothèques et extensions."},
        {"Checking…", "Vérification…"},
        {"Check all", "Tout vérifier"},
        {"No updates found.", "Aucune mise à jour détectée."},
        {"No projects match your search.", "Aucun projet ne correspond à la recherche."},
        {"Select a project to see its versions.", "Sélectionne un projet pour afficher ses versions."},
        {"PROJECT", "PROJET"},
        {"Update available", "Mise à jour disponible"},
        {"Installed by CyHub", "Installé dans CyHub"},
        {"Package available", "Paquet disponible"},
        {"Versions", "Versions"},
        {"Installed", "Installée"},
        {"No stable release", "Aucune publication stable"},
        {"Not checked", "À vérifier"},
        {"Downloading…", "Téléchargement en cours…"},
        {"Verifying and installing…", "Vérification et installation…"},
        {"Request accepted…", "Demande prise en charge…"},
        {"Update", "Mettre à jour"},
        {"Install", "Installer"},
        {"Start the CyHub Updater Windows service to install this package.", "Démarre le service Windows CyHub Updater pour installer ce paquet."},
        {"See the release on GitHub.", "Publication à consulter sur GitHub."},
        {"Open folder", "Ouvrir le dossier"},
        {"Open application", "Ouvrir l’application"},
        {"Check on GitHub", "Vérifier sur GitHub"},
        {"View release", "Voir la publication"},
        {"Open repository", "Ouvrir le dépôt"},
        {"Local project", "Projet local"},
        {"MCP Skill", "Skill MCP"},
        {"Read skill", "Lire le skill"},
        {"Copy Markdown", "Copier le Markdown"},
        {"Skill copied.", "Skill copié."},
        {"Clipboard unavailable.", "Presse-papiers indisponible."},
        {"Close", "Fermer"},
        {"The manual is missing from the CyHub folder.", "Manuel absent du dossier CyHub."},
        {"Request sent to the service.", "Demande envoyée au service."},
        {"Search for a project…", "Rechercher un projet…"},
        {"Could not open the link.", "Impossible d’ouvrir le lien."},
        {"Could not save your language preference.", "Impossible d’enregistrer la langue choisie."},
        {"RECOMMENDED COMPANION", "APPLICATION RECOMMANDÉE"},
        {"Install CyMCPHub", "Installer CyMCPHub"},
        {"Installing CyMCPHub…", "Installation de CyMCPHub…"},
        {"Update CyMCPHub", "Mettre à jour CyMCPHub"},
        {"Open CyMCPHub", "Ouvrir CyMCPHub"},
        {"CyMCPHub is ready to open.", "CyMCPHub est prêt à être ouvert."},
        {"CyMCPHub is already running. Quit it from the tray and reopen here to load the bridge or a new version.",
         "CyMCPHub est déjà ouvert. Quitte-le depuis la zone de notification et rouvre-le ici pour charger le pont ou une nouvelle version."},
        {"Available version", "Version disponible"},
        {"No Windows release is accessible on GitHub. Private releases require GH_TOKEN.",
         "Aucune publication Windows n’est accessible sur GitHub. Les publications privées nécessitent GH_TOKEN."},
    });
}

std::string localized_diagnostic(std::string_view message) {
    const bool french = cyui::locale() == "fr";
    for (const auto& [en, fr] : diagnostics) {
        const auto source = french ? en : fr;
        const auto target = french ? fr : en;
        if (diagnostic_prefix(message, source))
            return std::string(target) + std::string(message.substr(source.size()));
    }
    return std::string(message);
}
}

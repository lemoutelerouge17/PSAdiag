# PSADiag – Diagnostic OBD-II (Qt 6)

Application de bureau Qt 6 / C++17 pour l'adaptateur **OBDLink SX** (puce STN11xx) et
les clones **ELM327**. Projet complet et consolidé à partir des lots 1 à 8 du document
de conception, avec les corrections nécessaires pour qu'il fonctionne réellement.

> Outil **OBD-II générique** : lecture des capteurs, des codes défauts et du VIN.
> Aucune programmation, aucun télécodage, aucun accès sécurisé (SeedKey) aux calculateurs.

## Fonctions

| Onglet | Contenu |
|---|---|
| **Tableau de bord** | 4 jauges animées (régime, vitesse, température, papillon), tension batterie, charge, admission, MAF, pression carburant, VIN, nombre de défauts, trajet en cours |
| **Graphiques** | Courbes temps réel multi-capteurs, double axe, molette = zoom, glisser = zoom sur zone, double-clic = réinitialiser, curseur d'analyse au survol, clic sur la légende pour masquer une courbe |
| **Défauts (DTC)** | Lecture modes 03 + 07, effacement mode 04 (avec confirmation), ~150 descriptions françaises, recherche, export CSV |
| **Historique** | Sessions SQLite, tableau filtrable et triable, relecture dans le graphique, exports CSV / PDF / Excel |
| **Statistiques** | Min / max / moyenne de toutes les grandeurs (session en cours ou historique) |
| **Trajets** | Démarrer / arrêter, distance (intégration sur le temps réel), vitesses moyenne et max, régime moyen, carburant estimé |
| **Véhicules** | Multi-véhicules, création automatique à la lecture du VIN, fiche éditable, statistiques cumulées |
| **Journal** | Journal horodaté (fichier quotidien), affichage des trames TX/RX, terminal de commandes AT / OBD-II |

Également : **mode simulation** (aucun matériel requis), détection automatique des ports
(OBDLink présélectionné), alarmes de seuil configurables, thèmes sombre / clair mémorisés,
sauvegarde automatique quotidienne de la base (30 conservées), restauration, import / export,
migrations de schéma SQLite, rapport PDF avec statistiques, graphique et tableau.

## Compiler

### Prérequis
- **Qt 6.5 ou plus récent** avec les modules **Qt Charts** et **Qt Serial Port**
  (à cocher dans l'installateur Qt, rubrique « Additional Libraries »)
- CMake 3.21+ et un compilateur C++17 (MSVC 2019/2022 ou MinGW fourni avec Qt)

### Avec Qt Creator (le plus simple)
1. *Fichier → Ouvrir un fichier ou projet…* → `CMakeLists.txt`
2. Choisir un kit Qt 6 (Desktop MSVC ou MinGW 64 bits)
3. *Compiler* puis *Exécuter* (Ctrl+R)

### En ligne de commande
```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.7.3\msvc2019_64
cmake --build build --config Release
ctest --test-dir build -C Release        :: tests du parseur
```

### Installateur Windows
`build_release.bat` compile, lance les tests, exécute `windeployqt` puis génère
`Output\PSADiag_Setup_1.0.0.exe` avec Inno Setup 6 (si `iscc` est dans le PATH).
Ajustez la variable `QTDIR` en tête du script.

### Compilation dans le cloud (optionnel)
Le fichier `.github/workflows/build.yml` compile l'application et l'installateur sur GitHub
Actions à chaque envoi : poussez le dossier dans un dépôt GitHub et récupérez
l'exécutable dans l'onglet *Actions → Artifacts*.

### Export Excel (.xlsx)
Désactivé par défaut (dépendance externe). Pour l'activer :
```bat
cmake -S . -B build -DPSADIAG_WITH_XLSX=ON ...
```
QXlsx est alors téléchargé automatiquement (connexion Internet requise à la configuration).
Sans cette option, l'export CSV (compatible Excel : séparateur `;`, virgule décimale) reste disponible.

## Utilisation
1. Branchez l'OBDLink SX, contact mis (moteur arrêté ou tournant).
2. Choisissez le port (*USB Serial Port*), débit **115200**, puis **Connecter**.
   Pour un ELM327 USB, essayez **38400**. Cochez **Simulation** pour tester sans voiture.
3. L'application initialise l'adaptateur, lit le VIN et les défauts, puis crée une session.
4. **F5** démarre / arrête l'acquisition (enregistrée dans la base). **F6** relit les défauts.

## Données
`%APPDATA%\PSADiag\PSADiag\`
- `psadiag.db` : base SQLite (véhicules, sessions, mesures, défauts, trajets, alarmes, exports)
- `Backups\` : sauvegardes (`VACUUM INTO`, cohérentes même base ouverte)
- `Logs\` : journaux quotidiens

Les réglages (port, thème, seuils, intervalle) sont dans le registre :
`HKCU\Software\PSADiag\PSADiag`.

## Architecture
```
src/
├── App/         MainWindow (orchestration)
├── Core/        Transport (interface), SerialManager, SimulatorTransport, Logger, AppInfo
├── OBD/         ObdCommand, ObdParser (multitrames ISO-TP, espaces on/off), ObdEngine (file + délais)
├── DTC/         DTC, DTCManager (décodage J2012), dictionnaire FR
├── Database/    DatabaseManager, MigrationManager (schéma v3), BackupManager
├── Models/      VehicleSample, Vehicle, Trip, DtcTableModel
├── Alerts/      AlarmManager (seuils QSettings, anti-rafale)
├── Trips/       TripManager
├── Export/      CsvExporter, PdfExporter, ExcelExporter
├── Utils/       StatisticsManager, ThemeManager
└── Widgets/     Jauges, graphique, onglets, dialogues
Tests/           tst_obdparser (QtTest)
Installer/       PSADiag.iss (Inno Setup)
```

## Principales corrections par rapport au document d'origine
- Les parseurs supposaient des espaces alors que l'init envoie `ATS0` → parseur unique
  qui gère les deux formats, l'écho, `SEARCHING...`, les réponses multi-calculateurs.
- Le VIN CAN arrive en **multitrame ISO-TP** (`014` / `0:` / `1:` …) → réassemblage.
- Le premier octet d'une réponse mode 03 en CAN est le **nombre de défauts** → pris en compte.
- La file de commandes n'avait **pas de délai d'expiration** (blocage au premier silence).
- La distance des trajets supposait 1 mesure/s → intégration sur le temps réellement écoulé.
- `addDTC`, `setDTCs`, `saveTrip`… étaient déclarés mais jamais implémentés.
- Jauge : aiguille de longueur fixe (90 px) → dessin à l'échelle du widget.
- `AppId` Inno Setup invalide → GUID réel.
- Sauvegarde par copie de fichier d'une base ouverte (risque de corruption) → `VACUUM INTO`.

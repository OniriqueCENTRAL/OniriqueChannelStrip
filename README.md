# ONIRIQUE CHANNEL STRIP — channel strip vocal (VST3)

*(anciennement ONIRIQUE BASED)*

## Renommage + DA ONIRIQUE (dernière passe)

- **Nouveau nom** : ONIRIQUE CHANNEL STRIP (cible CMake `OniriqueChannelStrip`,
  `PRODUCT_NAME`, `getName()`, classes `OniriqueChannelStripProcessor` /
  `OniriqueChannelStripEditor`). Le `PLUGIN_CODE` passe de `Obd1` à `Ocs1` :
  le DAW le voit comme un nouveau plugin (les projets enregistrés avec
  l'ancien ONIRIQUE BASED continuent de pointer vers l'ancien .vst3).
- **Même DA d'interface que ONIRIQUE VOCALBOX** : fond nuit / sang avec
  halo braise, cartes en verre sombre à filet fin (point braise + titre),
  potards à arc braise lumineux (depuis le centre pour les gains ±) et corps
  sphérique sombre, boutons power à anneau lumineux à la place des
  interrupteurs de bypass, sélecteurs segmentés à pastille braise qui
  glisse, pastilles LED (AUTO, LISTEN), meters à remplissage os → braise,
  écran de saturation identique à l'afficheur RESONANCE du VocalBox.
  En-tête vivant : sigil ONIRIQUE (même asset que le VocalBox) qui respire,
  étoiles qui scintillent, impulsion sur la ligne de signal, et chaîne de
  traitement **réelle** affichée sous le titre (suit les badges d'ordre).
- **Animations** (comme le VocalBox) : les arcs se déploient à l'ouverture,
  survol / réglage d'un potard = l'arc s'illumine et le nom laisse place à
  une pastille de valeur ; module coupé = voile nuit en fondu sur sa carte
  (titre, power et badge restent nets). Les timers d'animation ne tournent
  que pendant une transition ; fond et ombres portées sont mis en cache
  (une image par taille de fenêtre).
- Aucun changement DSP ni de paramètre.


Channel strip vocal complet, 8 modules, ordre de traitement entièrement
reconfigurable, interface graphique sur mesure. **Étape 4 (finalisation)** :
audit stabilité / CPU / zéro-allocation, anti-clics bypass et
réordonnancement, oversampling + latence déclarée, presets d'usine.

## Retouches demandées (dernière passe)

- **Noise Gate** : ajout de deux petits potards **ATTACK** et **RELEASE** à
  côté du THRESHOLD (style NS1). hold/range/hysteresis restent masqués et
  automatisables depuis l'hôte.
- **SSL Channel** : les sections **EQ** et **COMPRESSOR** sont maintenant
  étiquetées (titres soulignés d'un filet), en plus des sous-titres de bande
  LF/LMF/HMF/HF déjà présents — place dans la 3ᵉ colonne (libre) des rangées
  LF et HF, qui n'ont que 2 potards réels chacune.
- **De-Esser** : bouton **LISTEN** (style Waves) sur chaque instance —
  isole et laisse entendre exactement ce que le de-esser retire (la
  différence entre le signal avant traitement et le signal issu des bandes
  cloche dynamiques), indépendamment du réglage Mix. Nouveau paramètre
  `<id>_listen` (bool, off par défaut) ; n'affecte la sortie que lorsqu'il
  est activé, sans coût ni changement de comportement sinon.
- **Saturation** : ajout de potards **IN** et **OUT** pour gérer les
  niveaux. IN est un nouveau paramètre `saturation_input` (trim pur, ±20 dB,
  appliqué avant le Drive) — OUT réutilise le paramètre `output` déjà
  existant, simplement renommé à l'écran et repositionné dans la nouvelle
  grille à 3 colonnes (IN/DRIVE/TONE en haut, MIX/OUT en bas).

## Finalisation (étape 4) — résumé

- **Zéro allocation dans processBlock()** : `ProcessorModule::getParam()`
  utilisait une concaténation de `juce::String` + une recherche par nom dans
  l'apvts à *chaque appel* — donc à chaque bloc. Corrigé par un cache de
  pointeurs atomiques, "chauffé" une fois dans `prepare()` de chaque module
  (hors thread audio). `ModuleChain` ne trie plus via `std::stable_sort`
  (dont le besoin de mémoire temporaire n'est pas garanti nul par la norme)
  mais via un tri par insertion manuel, sans allocation.
- **Anti-clics bypass** : chaque module a un gain lissé (~8 ms) qui fait un
  vrai fondu enchaîné dry/wet au moment où son bypass change, plutôt qu'un
  saut brutal. Coût nul une fois la transition terminée (module bypassé =
  plus du tout appelé).
- **Tout bypassé par défaut à l'ouverture du plugin** : les 8 paramètres
  `<id>_bypass` ont leur valeur par défaut à `true` (`ProcessorModule.h` →
  `addCommonParameters`) — à l'insertion du plugin, la chaîne est
  transparente (signal sec inchangé) et chaque module s'active
  individuellement via son interrupteur ON/OFF, au fur et à mesure. Un
  preset d'usine qui active des modules reste prioritaire une fois chargé
  (voir Presets ci-dessous).
- **Anti-clics réordonnancement** : un changement d'ordre de traitement crée
  un chemin de signal différent d'un bloc à l'autre, ce qu'un simple
  crossfade ne peut pas masquer proprement — un court fondu de sortie
  (~6 ms) atténue la discontinuité au lieu de la laisser passer brute.
- **Oversampling** (Saturation uniquement) : OFF / 2X / 4X / 8X, **OFF par
  défaut**. Filtre IIR demi-bande (coût/latence modérés). Latence déclarée
  à l'hôte via `setLatencySamples()`, toujours depuis un contexte sûr
  (`prepareToPlay()` ou `AsyncUpdater`, jamais depuis le thread audio).
- **Presets** : 9 presets d'usine (voir plus bas), exposés via le mécanisme
  standard `AudioProcessor::getNumPrograms/setCurrentProgram` — visibles
  dans le sélecteur de presets natif de n'importe quel DAW.
- **Robustesse multi-sample-rate** : `prepareToPlay` sécurise `sampleRate`/
  `samplesPerBlock` contre des valeurs dégénérées ; toutes les fréquences de
  filtre restent bornées sous Nyquist à n'importe quel sample rate (voir
  `DSPHelpers::safeFilterFreq`, inchangé, déjà correct).

## Ce qui est livré

- **Moteur de chaîne reconfigurable** (`DSP/ModuleChain`) : chaque module a un
  paramètre `<id>_order` (1 à 8). À chaque bloc audio, les modules sont triés
  par cette valeur puis traités dans cet ordre — indépendamment de leur
  disposition graphique, qui restera fixe plus tard.
- **8 modules DSP indépendants**, tous dérivés de `ProcessorModule` :
  `NoiseGateModule`, `SSLChannelModule`, `DeEsserModule` (×2, De-Esser 1 et 2),
  `SaturationModule`, `FETCompressorModule` (1176), `VariMuCompressorModule`
  (Fairchild 670), `ParametricEQModule`.
- **Paramètres automatisables et sauvegardés** via
  `juce::AudioProcessorValueTreeState` : chaque module expose ses propres
  paramètres (préfixés par son identifiant) plus les deux paramètres communs
  `order` et `bypass`.
- **Interface provisoire** : liste générique scrollable (un slider / menu /
  case à cocher par paramètre), juste pour pouvoir tester et automatiser
  chaque module dès maintenant. Son remplacement par la maquette finale ne
  touchera à rien côté moteur audio.

## Architecture

```
Source/
  PluginProcessor.h/.cpp   Point d'entree JUCE : APVTS + ModuleChain, prepareToPlay/processBlock
  PluginEditor.h/.cpp      Interface provisoire (liste generique de parametres)
  Parameters.h/.cpp        Identifiants des modules + assemblage de tous les parametres

  DSP/
    ProcessorModule.h          Interface commune (prepare / process / reset / bypass / order)
    ModuleChain.h/.cpp         Conteneur qui trie et traite les modules selon "order"
    DSPHelpers.h                Petits utilitaires partages (ranges de frequence, labels)

    NoiseGateModule.h/.cpp
    SSLChannelModule.h/.cpp
    DeEsserModule.h/.cpp         (classe unique, instanciee 2x : De-Esser 1 et 2)
    SaturationModule.h/.cpp
    FETCompressorModule.h/.cpp
    VariMuCompressorModule.h/.cpp
    ParametricEQModule.h/.cpp
```

### Comment fonctionne le réordonnancement

Chaque module lit sa position via `ProcessorModule::getOrder()`, qui lit le
paramètre hôte `<id>_order` (automatisable, sauvegardé avec le projet).
`ModuleChain::process()` trie les 8 modules par cette valeur à chaque bloc
(coût négligeable, 8 éléments) puis les traite dans cet ordre. Changer
l'ordre de traitement — à la souris ou par automation — ne demande donc
aucune logique supplémentaire : il suffit de changer la valeur du paramètre.

Le bypass individuel fonctionne pareil : `ModuleChain` ne traite pas les
modules dont `isBypassed()` (paramètre `<id>_bypass`) est actif.

### Ajouter un neuvième module plus tard

1. Créer `MonModule.h/.cpp` dans `DSP/`, dérivé de `ProcessorModule`.
2. Lui donner une méthode statique `addParameters(layout, id)` qui appelle
   `addCommonParameters(...)` puis ajoute ses propres paramètres.
3. L'ajouter dans `Parameters.cpp` (`createParameterLayout`) et dans
   `PluginProcessor.cpp` (`buildModuleChain`).
4. Ajouter les deux fichiers dans `CMakeLists.txt`.

Rien d'autre à changer : le tri, le bypass et la sauvegarde d'état sont
génériques.

## Les 8 modules — comportement DSP

Tous les modules partagent trois garanties de stabilité numérique (voir
`DSP/DSPHelpers.h`) : les fréquences de filtre sont bornées près de Nyquist
(`safeFilterFreq`), les Q sont bornés (`safeQ`), et chaque sortie de module
est passée dans `sanitizeBlock` (NaN/Inf → 0, écrêtage à ±8.0) — en plus
d'une seconde passe systématique au niveau de `ModuleChain` après chaque
module. Les coefficients de filtre IIR ne sont recalculés que si au moins un
paramètre a réellement changé (`FilterParamCache`), pour économiser le CPU.

| Module | Chaîne interne | Paramètres | Caractère recherché |
|---|---|---|---|
| **Noise Gate** | Détecteur de niveau (ballistique rapide) → machine à états seuil/hystérésis/hold → gain lissé attack/release, par échantillon | threshold, attack, hold, release, range, hysteresis | Coupe souffle/bruit de fond sans tronquer les fins de mots : le hold maintient la porte ouverte, l'hystérésis évite le "chatter" |
| **SSL Channel** | Trim → HPF → EQ 4 bandes (low shelf / low-mid bell / high-mid bell / high shelf) → compresseur → gate/expander doux → output | input, hpf, 4×(freq/gain/Q), threshold/ratio/attack/release/makeup, gateThreshold/gateRange/gateAttack/gateRelease, output | Voix présente, médiums propres, aigu précis, attaque nette |
| **De-Esser 1 / 2** (même classe, 2 instances indépendantes) | Détection double bande (bande principale "S" + bande compagnon ~×1.6 pour "SH/CH/T/Z") → enveloppe attack/release → 2 bandes cloche dynamiques → mix dry/wet, + mode LISTEN (voir plus bas) | frequency, threshold, range, attack, release, mix, listen, bypass | Réduction musicale sans effet de "cheveu sur la langue" : la bande compagnon capte les consonnes plus graves sans sur-creuser la bande principale |
| **Saturation / Transformer** | Trim d'entrée → pré-gain (drive + densité dépendante du niveau) → waveshaper (Tape/Tube/Transformer) → bloqueur DC → tonalité (shelf haut) → compensation auto partielle → output | input, drive, type, tone, mix, output | Chaleur, densité harmonique, coloration progressive avec le niveau, compression naturelle légère — façon transformateur analogique |
| **1176 FET** | Input → compresseur rapide (ratio 4/8/12/20:1, attack/release très courts) → mesure de réduction (si Auto Gain) → output compensé → mix | input, threshold, ratio, attack, release, output, mix, autoGain | Attaque/relâche quasi instantanés, punch, densité, compression agressive possible |
| **Fairchild 670 / Vari-Mu** | Input → compresseur lent (attack/release larges, ratio doux) → saturation tube proportionnelle à la réduction mesurée → output → mix | input, threshold, attack, release, ratio, output, mix | Doux, épais, musical, effet "glue" — nettement différent du FET |
| **EQ** | 8 bandes indépendantes (Bell / Low Shelf / High Shelf / Low Cut / High Cut / Notch), + analyseur de spectre FFT 2048 pts en sortie | 8×(freq, gain, Q, type) | EQ propre et précis ; `getSpectrumAnalyzer()` expose les données pour une future interface graphique |

Aucun algorithme ni code propriétaire SSL / UA / Fairchild / FabFilter /
Kazrog / Waves n'a été utilisé : chaque module est une implémentation
originale basée sur les briques DSP standard de JUCE (`dsp::Compressor`,
`dsp::IIR`, `dsp::WaveShaper`, `dsp::FFT`...) et des machines à états/
enveloppes écrites pour ce projet, inspirée uniquement par le comportement
sonore et fonctionnel attendu de ce type d'appareil.

## Interface graphique (étape 4 — refonte complète)

L'interface a été entièrement refaite sur la base du pack de design fourni
(`ONIRIQUE_UI_BRIEF.md` + `ui-spec.json` + maquette HTML de référence) :
disposition **absolue**, aux coordonnées exactes de la maquette 1000×640,
rendu **3D vectoriel** (`juce::Path` / `ColourGradient` / `DropShadow` —
aucun OpenGL, aucune image pré-rendue hormis le logo). Aucun changement DSP
hormis les getters de meters (§6 du brief) ajoutés aux modules concernés.

```
┌──────────┬──────────────────────┬───────────────────────┐
│          │ DE-ESSER (1 et 2)    │ SATURATION             │
│   SSL    ├──────────────────────┼───────────────────────┤
│ CHANNEL  │ FAIRCHILD 670        │ 1176 FET               │
├──────────┼──────────────────────┴───────────────────────┤
│NOISE GATE│ EQ                                            │
└──────────┴───────────────────────────────────────────────┘
```

Fenêtre redimensionnable à **ratio fixe 1000:640** (ouverture à 1400×896,
`ComponentBoundsConstrainer`) : toute la composition est mise à l'échelle
uniformément, aucune mise en page n'est recalculée dynamiquement.

- **Palette** (`GUI/UIStyle.h`) : un accent par module (bordeaux/braise/prune
  sombre), repris exactement de `ui-spec.json → palette`. Aucune couleur en
  dur ailleurs dans le code de l'interface.
- **Potards** (`GUI/ParamControls.h` + `GUI/OniriqueLookAndFeel`) : anneau
  biseauté, calotte bombée, reflet, pointeur lumineux et couronne de 9
  graduations, dessinés dans `OniriqueLookAndFeel::drawRotarySlider`. Les
  graduations min/max affichées sous chaque potard viennent du **vrai**
  `NormalisableRange` du paramètre (règle 5 du brief), jamais de la
  maquette — y compris quand les deux divergent (ex: Fairchild/1176, où les
  bornes "mockup" ne correspondent pas aux vraies plages ms/s). Pendant le
  glissement, la valeur numérique formatée (Hz→k, dB signé, etc.) remplace
  le nom du potard. Double-clic = valeur par défaut du paramètre.
- **Noise Gate** (style **"NS1" de Waves**) : un seul gros potard
  **THRESHOLD** affiché, plus un meter de réduction de gain vertical
  juste à côté. Les 5 autres paramètres (attack, hold, release, range,
  hysteresis) gardent leurs valeurs par défaut et restent pleinement
  automatisables depuis l'hôte — ils ne sont simplement plus affichés ici,
  aucun paramètre n'est retiré côté DSP.
- **Bypass** : interrupteur à bascule **ON/OFF** (piste + bille coulissante,
  texte "ON"/"OFF" en creux) dans l'en-tête de chaque module, à la place de
  l'ancienne LED ronde — même emplacement, même attache au paramètre
  `<id>_bypass` (`ButtonParameterAttachment`, donc même automatisation hôte,
  rien de changé côté DSP). Un module bypassé **s'assombrit en plus à 40 %
  d'opacité** (`setAlpha` sur tout le panneau, donc tous ses enfants) — plus
  de texte "BYPASSED" superposé.
- **Badge d'ordre** : inchangé dans sa logique (`GUI/ModuleOrderManager`),
  restylé en petit afficheur en creux.
- **Boutons segmentés** (`GUI/ParamControls::ParamSegmented`) : 1176 (ratio
  4:1/8:1/12:1/20:1), Saturation (TAPE/TUBE/XFMR) — relief enfoncé/allumé
  sur l'option active, exclusifs.
- **Meters** (`GUI/MeterBar`, nouveau) : tube en creux, segments lumineux,
  ballistique montée instantanée / descente ~300 ms / peak-hold ~500 ms,
  rafraîchi à 30 Hz et redessiné seulement si la valeur affichée change de
  plus de 0,1 dB. Présents sur SSL (GR horizontal), Fairchild et 1176 (GR
  vertical), les deux De-Essers (niveau/threshold + GR), et IN/OUT dans la
  barre du haut. Le trait blanc du meter de niveau du De-Esser est
  **déplaçable à la souris** et pilote directement le paramètre threshold.
  Côté DSP (seul changement autorisé) : un `std::atomic<float>` par module
  concerné, écrit une fois par bloc, exposé par un getter `const noexcept`
  (`getGainReductionDb()`, `getDetectorLevelDb()`,
  `getInputPeakDb()`/`getOutputPeakDb()` sur le processeur) ; un module
  bypassé voit son meter retomber à zéro (`ProcessorModule::resetMeters()`).
- **Saturation** (`GUI/TransferCurveDisplay`, nouveau) : courbe de transfert
  tracée avec `SaturationModule::previewShape()` — exactement la même
  fonction que le DSP réel — redessinée quand drive/type/tone changent.
- **EQ** (style **"MAAG EQ4"**, `GUI/ModulePanel` standard, remplace l'ancien
  écran courbe + billes déplaçables) : 8 gros potards boost/cut à fréquence
  fixe, un par bande (80 CUT, 150 LOW, 400, 900, 2.5k, 5k, 9k HIGH, 14k AIR —
  les mêmes 8 fréquences par défaut que `ParametricEQModule::bandDefaults`),
  chacun lié uniquement au paramètre `bandN_gain`. Les paramètres
  `freq`/`Q`/`type` de chaque bande restent inchangés et automatisables côté
  hôte ; ils garde simplement leurs valeurs par défaut et ne sont plus
  affichés ici — aucun paramètre retiré côté DSP, seule la présentation
  change (règle 5 du brief). Pas de texte "MAAG"/"EQ4" sur le panneau
  (le titre reste "EQ"), conformément à la règle anti-marque du projet.
- **De-Esser** (`GUI/DualDeEsserPanel`, nouveau) : les deux instances
  (toujours indépendantes) dans un seul panneau graphique, deux lignes avec
  chacune sa LED, son badge d'ordre (3 et 8 par défaut), ses 5 potards et
  ses deux meters.
- **Logo** (`GUI/LogoView`, remplacé depuis par le sigil animé de l'en-tête) : `assets/logo.png` embarqué en
  `BinaryData` (`juce_add_binary_data`), cadre métallique biseauté, reflet.
- **Performance** : `setBufferedToImage(true)` sur les panneaux (fond
  statique mis en cache, redessiné seulement sur changement de bypass ou de
  taille) ; aucune animation décorative hors meters/analyseur.

### Écart assumé par rapport à la maquette

La maquette prévoit un emplacement "Q" pour les bandes Low Shelf et High
Shelf du SSL Channel, mais ces filtres en plateau n'ont pas de paramètre Q
réel côté DSP (Q fixe à 0.707, voir `SSLChannelModule::updateFiltersIfNeeded`)
— conformément à la règle 5 du brief ("les vraies plages gagnent"), ces 2
emplacements sont simplement laissés vides plutôt que d'inventer un
paramètre fictif. Le SSL Channel affiche donc 19 potards réels sur les 21
emplacements de la maquette (21 + les 5 De-Esser ×2 + 4 Saturation + 7
Fairchild + 6×1176 + 6 Noise Gate = 54 potards au total, conforme à l'§9 du
brief). Par ailleurs, les graduations des meters suivent un espacement
linéaire en dB (plus simple et tout aussi lisible) plutôt que la courbe
légèrement compressée de la maquette de référence.

### Fichiers

```
Source/GUI/
  UIStyle.h               DA ONIRIQUE : palette nuit/sang/braise, polices, lueurs, carte, pastille
  OniriqueLookAndFeel.*   Potard arc braise + corps spherique, combos, menus
  ModuleOrderManager.*    Logique de reordonnancement propre (permutation 1..8) -- inchange
  OrderBadge.*            Badge d'ordre en pastille (chevrons braise au survol)
  ParamControls.*         ParamKnob / PowerSwitch / PillButton / ParamToggle / ParamSegmented / ParamCombo / SectionLabel
  ModulePanel.*           Carte a positionnement absolu (maquette) + voile de bypass anime + addExtra()
  MeterBar.*              Meter horizontal/vertical (os -> braise), ballistique, marqueur deplaçable
  TransferCurveDisplay.*  Ecran de courbe de transfert (Saturation)
  DualDeEsserPanel.*      Panneau fusionne des deux instances de De-Esser
  Assets/sigil.png        Sigil ONIRIQUE (BinaryData), meme asset que le VocalBox
```

### Chaîne par défaut

Gate(1) → SSL Channel(2) → De-Esser 1(3) → Saturation(4) → 1176(5) →
Fairchild(6) → EQ(7) → De-Esser 2(8). Le second de-esser en toute fin de
chaîne rattrape la sibilance éventuellement ravivée par la saturation/EQ —
une pratique courante en mixage vocal. Cet ordre n'est qu'un point de départ :
il se change librement via le paramètre `<id>_order` de chaque module.

## Presets

9 presets d'usine (`Source/Presets.cpp`), visibles directement dans le
sélecteur de presets du DAW (aucune interface supplémentaire nécessaire) :
Default, Clean Vocal, Rap Vocal, Aggressive Vocal, Warm Vocal, Lead Vocal,
Adlib, Dark Vocal, Airy Vocal.

Chaque preset ne liste que les paramètres qui le caractérisent ; charger un
preset réinitialise d'abord tout aux valeurs par défaut puis applique ces
overrides (recall reproductible). Pour ajouter un preset : une entrée dans
`Presets.cpp`, rien d'autre à changer.

## Compromis assumés (à connaître)

- **Latence fixe par réglage, pas par état de bypass** : la latence déclarée
  à l'hôte reflète le réglage d'oversampling choisi, pas si la Saturation
  est actuellement bypassée. Faire varier la latence à chaque bypass
  provoquerait des micro-coupures dans la plupart des hôtes — un compromis
  délibéré en faveur de la stabilité (priorité n°1 demandée).
- **Changement d'oversampling = une allocation ponctuelle** : passer de OFF
  à 2X/4X/8X (ou entre eux) reconstruit l'objet `dsp::Oversampling` au
  prochain bloc traité, donc alloue une fois à cet instant précis. C'est un
  réglage discret que l'utilisateur change rarement et délibérément (pas une
  valeur automatisée en continu) ; éviter même cette allocation isolée
  demanderait une machinerie inter-thread nettement plus complexe pour un
  bénéfice marginal — non retenu pour privilégier la fiabilité du code.
- **Réordonnancement** : le fondu de sortie masque la discontinuité mais ne
  la supprime pas entièrement par construction (changer l'ordre change le
  chemin du signal, ce n'est pas un simple crossfade dry/wet comme pour le
  bypass).

## Compilation (Windows 10/11, VS2022)

Prérequis : Visual Studio 2022 (Développement Desktop en C++), CMake 3.22+,
Git, connexion Internet à la première configuration (télécharge JUCE 8).

    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

Plugin généré dans :

    build\OniriqueChannelStrip_artefacts\Release\VST3\ONIRIQUE CHANNEL STRIP.vst3

Copier ce dossier dans `C:\Program Files\Common Files\VST3\`, puis dans FL
Studio : **Options → Manage plugins → Find installed plugins**.

## Prochaine étape

Construire l'interface graphique finale d'après la maquette : disposition
fixe (SSL à gauche en haut, Noise Gate en bas à gauche, 2 De-Esser empilés au
centre en haut, Saturation en haut à droite, 1176 au centre, Fairchild à
droite du 1176, EQ en bas à droite), avec dans chaque module le petit champ
numérique d'ordre en haut à droite, relié au paramètre `<id>_order` déjà en
place. Dites-moi quand vous voulez attaquer cette partie.

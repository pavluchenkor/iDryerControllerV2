# Démarrage rapide

Un parcours court, de l'impression des pièces à un sécheur fonctionnel :
impression du boîtier, câblage des modules, flashage, association au portail
(claim) et réglage initial du volet. Les détails de chaque étape figurent dans
les sections dédiées de la documentation.

## Ce qu'il vous faut

- Carte contrôleur (RP2040) et le module **iDryer-Link** (ESP32-C3 Super Mini).
- Un câble USB de données.
- Un navigateur Chromium (Chrome, Edge) prenant en charge WebUSB.
- Un réseau Wi-Fi 2,4 GHz avec mot de passe.
- Un compte sur le portail — <https://portal.idryer.org>.

## 1. Câblage

!!! warning "Testez d'abord l'assemblage sur l'établi"
    Avant l'assemblage final, réunissez tous les composants sur l'établi et
    vérifiez que l'appareil fonctionne. Les erreurs de câblage sont plus faciles
    à trouver tant que vous avez encore accès à chaque pièce.

!!! danger "Alimentation"
    Ne branchez ni ne débranchez jamais de modules (Link, écran, capteurs) sous
    tension. Effectuez tout câblage appareil hors tension.

Câblez les modules et composants conformément à la section dédiée de la
documentation. Link doit être connecté au contrôleur avant le flashage.

!!! warning "N'intervertissez pas les fils"
    Le câblage paraît simple, mais les fils vers la carte sont souvent
    intervertis. De telles erreurs sont difficiles à diagnostiquer à distance :
    le sécheur semble fonctionner normalement, mais la logique de régulation
    change. Par exemple, si le ventilateur et le chauffage sont intervertis,
    l'appareil semble fonctionner, mais le régulateur PID ne pilote pas le
    chauffage — le chauffage tourne en permanence à pleine puissance.

## 2. Flasher le contrôleur et Link

Le flashage se fait depuis un navigateur sur <https://install.idryer.org>.
Suivez les étapes de l'assistant dans l'ordre :

1. **Flash Controller** — branchez l'USB sur le port du contrôleur, placez la
   carte en mode `BOOTSEL` et flashez le contrôleur.
2. **Flash Link** — déplacez le câble USB vers le port Link (Link reste connecté
   au contrôleur) et flashez le module.

## 3. Wi-Fi et association au portail

Restez dans le même assistant sur <https://install.idryer.org> :

1. **Wi-Fi** — après le flashage de Link, l'assistant de configuration réseau
   (Improv) s'ouvre. Saisissez le nom (SSID) et le mot de passe de votre réseau
   Wi-Fi.
2. **Claim** — lancez l'association. L'assistant affiche un `PIN`.
3. **Portail** — ouvrez <https://portal.idryer.org>, connectez-vous, ajoutez un
   appareil sur la page des appareils et saisissez le `PIN`.

Après l'association, l'appareil apparaît dans la liste du portail.

## 4. Impression des pièces du boîtier

Imprimez les pièces du boîtier avec les paramètres indiqués dans la section CAD
de la documentation. Ces paramètres ont été éprouvés sur des milliers de
montages. Si vous vous en écartez, le boîtier perd son isolation thermique et le
sécheur n'atteint pas sa température de fonctionnement.

## 5. Volet et servomoteur

Vous pouvez régler le volet depuis l'écran du contrôleur (menu `SETTINGS →
SERVO`), depuis les réglages de l'appareil sur le portail ou depuis
l'application.

!!! warning "Ordre de pose du volet"
    Réglez d'abord l'angle, puis posez le volet — sinon il butera contre le
    boîtier et bloquera le servomoteur.

1. Réglez `CLOSED ANGLE = 0`. Le servomoteur se place dans cette position
   (aperçu).
2. En fonction de la position réelle de l'axe, posez le volet de sorte qu'en
   position fermée il obture entièrement <!-- TODO: confirmer le terme —
   ouverture/conduit de l'ensemble du volet --> le canal d'air de l'ensemble du
   volet.
3. Réglez `OPEN ANGLE` selon votre mécanique. Cette étape peut aussi être
   effectuée après l'assemblage final.

## 6. Régulateur PID du chauffage

Le firmware est déjà fourni avec des valeurs de régulateur PID fonctionnelles —
aucune calibration séparée n'est nécessaire pour le démarrage et la première
vérification. Si besoin, lancez l'autotune pour ajuster les coefficients à votre
montage.

## 7. Commande via le portail et l'application

Toutes les fonctions et tous les menus du contrôleur sont accessibles via le
portail et l'application. Le portail et l'application étendent nettement les
possibilités du sécheur : télémétrie, historique des données, préréglages et
commande à distance.

La commande est disponible depuis le portail <https://portal.idryer.org> ou
depuis l'application :

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

Pour lancer le séchage :

1. Ouvrez le portail ou l'application — la vignette de votre appareil apparaît à
   l'écran.
2. Sélectionnez le mode — séchage ou stockage.
3. Appuyez sur démarrer.

Les valeurs de température et de durée par défaut conviennent à la plupart des
cas. Modifiez-les selon votre matériau si nécessaire.

### Suivi du filament et avis

Chaque filament sur votre étagère est reflété sur le portail, et toutes les
données sont enregistrées. Vous pouvez laisser un avis pour chaque filament et
lire les avis d'autres utilisateurs. Les avis sont regroupés par fabricant, type
et autres attributs et sont disponibles directement sur le portail et le forum.

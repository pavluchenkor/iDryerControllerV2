# Schnellstart

Ein kurzer Weg vom Druck der Teile bis zum funktionierenden Trockner: Gehäuse
drucken, Module verkabeln, flashen, am Portal registrieren (Claim) und die
Klappe erstmalig einrichten. Details zu jedem Schritt stehen in den
entsprechenden Abschnitten der Dokumentation.

## Was Sie benötigen

- Controller-Platine (RP2040) und das Modul **iDryer-Link** (ESP32-C3 Super Mini).
- Ein USB-Datenkabel.
- Ein Chromium-Browser (Chrome, Edge) mit WebUSB-Unterstützung.
- Ein 2,4-GHz-WLAN mit Passwort.
- Ein Konto im Portal — <https://portal.idryer.org>.

## 1. Verkabelung

!!! warning "Zuerst den Aufbau auf dem Tisch prüfen"
    Bauen Sie vor der Endmontage alle Komponenten auf dem Tisch zusammen und
    stellen Sie sicher, dass das Gerät funktioniert. Verkabelungsfehler lassen
    sich leichter finden, solange Sie noch Zugang zu allen Teilen haben.

!!! danger "Stromversorgung"
    Schließen Sie Module (Link, Display, Sensoren) niemals bei anliegender
    Spannung an oder trennen Sie sie. Führen Sie jede Verkabelung im
    stromlosen Zustand durch.

Verkabeln Sie Module und Komponenten gemäß dem entsprechenden Abschnitt der
Dokumentation. Link muss vor dem Flashen mit dem Controller verbunden sein.

!!! warning "Adern nicht vertauschen"
    Die Verkabelung wirkt einfach, doch die Adern zur Platine werden oft
    vertauscht. Solche Fehler sind aus der Ferne schwer zu diagnostizieren: Der
    Trockner scheint normal zu arbeiten, aber die Regellogik ändert sich. Werden
    zum Beispiel Lüfter und Heizung vertauscht, scheint das Gerät zu
    funktionieren, doch der PID-Regler steuert die Heizung nicht — die Heizung
    läuft ständig auf voller Leistung.

## 2. Controller und Link flashen

Das Flashen erfolgt im Browser unter <https://install.idryer.org>. Folgen Sie
den Schritten des Assistenten der Reihe nach:

1. **Flash Controller** — schließen Sie USB an den Controller-Port an, versetzen
   Sie die Platine in den `BOOTSEL`-Modus und flashen Sie den Controller.
2. **Flash Link** — stecken Sie das USB-Kabel an den Link-Port um (Link bleibt
   mit dem Controller verbunden) und flashen Sie das Modul.

## 3. WLAN und Registrierung am Portal

Bleiben Sie im selben Assistenten unter <https://install.idryer.org>:

1. **WLAN** — nach dem Flashen von Link öffnet sich der Netzwerk-Assistent
   (Improv). Geben Sie den Netzwerknamen (SSID) und das WLAN-Passwort ein.
2. **Claim** — starten Sie die Registrierung. Der Assistent zeigt eine `PIN` an.
3. **Portal** — öffnen Sie <https://portal.idryer.org>, melden Sie sich an, fügen
   Sie auf der Geräteseite ein Gerät hinzu und geben Sie die `PIN` ein.

Nach der Registrierung erscheint das Gerät in der Liste im Portal.

## 4. Gehäuseteile drucken

Drucken Sie die Gehäuseteile mit den im CAD-Abschnitt der Dokumentation
angegebenen Parametern. Diese Parameter sind über Tausende von Aufbauten
erprobt. Bei Abweichungen verliert das Gehäuse seine Wärmedämmung, und der
Trockner erreicht seine Betriebstemperatur nicht.

## 5. Klappe und Servo

Die Klappe können Sie über das Controller-Display (Menü `SETTINGS → SERVO`), über
die Geräteeinstellungen im Portal oder in der App einrichten.

!!! warning "Reihenfolge beim Einbau der Klappe"
    Legen Sie zuerst den Winkel fest und montieren Sie erst dann die Klappe —
    sonst stößt sie am Gehäuse an und blockiert den Servo.

1. Stellen Sie `CLOSED ANGLE = 0` ein. Der Servo fährt in diese Position
   (Vorschau).
2. Montieren Sie die Klappe entsprechend der tatsächlichen Wellenposition so,
   dass sie in geschlossener Stellung <!-- TODO: Begriff prüfen —
   Öffnung/Schacht/Kanal der Klappeneinheit --> den Luftkanal der Klappeneinheit
   vollständig verschließt.
3. Stellen Sie `OPEN ANGLE` passend zu Ihrer Mechanik ein. Dieser Schritt kann
   auch nach der Endmontage erfolgen.

## 6. PID-Regler der Heizung

Die Firmware bringt bereits funktionierende Werte für den PID-Regler mit — für
den Start und die erste Prüfung ist keine separate Kalibrierung nötig. Führen Sie
bei Bedarf ein Autotune durch, um die Koeffizienten an Ihren Aufbau anzupassen.

## 7. Steuerung über Portal und App

Alle Funktionen und Menüs des Controllers sind über das Portal und die App
verfügbar. Portal und App erweitern die Möglichkeiten des Trockners erheblich:
Telemetrie, Datenverlauf, Presets und Fernsteuerung.

Die Steuerung ist über das Portal <https://portal.idryer.org> oder über die App
möglich:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

So starten Sie die Trocknung:

1. Öffnen Sie das Portal oder die App — die Kachel Ihres Geräts erscheint auf
   dem Bildschirm.
2. Wählen Sie den Modus — Trocknen oder Lagern.
3. Drücken Sie Start.

Die Standardwerte für Temperatur und Zeit sind für die meisten Fälle passend
gewählt. Ändern Sie sie bei Bedarf für Ihr Material.

### Filament-Erfassung und Bewertungen

Jedes Filament in Ihrem Regal wird im Portal abgebildet, und alle Daten werden
erfasst. Zu jedem Filament können Sie eine Bewertung abgeben und Bewertungen
anderer Nutzer lesen. Die Bewertungen sind nach Hersteller, Typ und weiteren
Merkmalen gruppiert und direkt im Portal und im Forum verfügbar.

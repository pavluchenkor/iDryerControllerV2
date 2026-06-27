# iDryerControllerV2

Diese Dokumentation beschreibt die Firmware, die Verdrahtung und die Konfiguration des iDryer-Steuerboards.

Das Projekt steht Enthusiasten offen: Die Firmware kann auf RP2040-Mikrocontrollern ausgeführt werden, und mit dem Anschluss der erforderlichen Peripherie ist der Steuercontroller vollständig funktionsfähig.

## Grundlegende Firmware-Funktionen

Verwaltet die Heizung, den Lüfter und den Servomotor, liest Temperatur-, Feuchte-, Gewichts- und RFID-Reader-Werte. Mit ESP32 als verfügbarem Wi-Fi-Modul (iDryer-Link) wird die Funktionalität des Steuercontrollers um Telemetrie und kabellose Steuerung erweitert. Die Steuerung kann lokal aus der Anwendung erfolgen, und beim Anschluss an das Portal sendet das Gerät Telemetrie, empfängt Befehle und führt eine Datenhistorie. Diese Daten sind besonders nützlich für die Analyse des Trockenprozesses und die Beurteilung des Fadenzustands.

## Was hier enthalten ist

In dieser Dokumentation werden folgende Themen behandelt:

- **Installation und Konfiguration** — Firmware-Programmierung des Steuercontrollers über WebUSB und Portkonfiguration
- **Hardwareverbindung** — Schaltpläne für Display-, Sensor- und Modulverbindungen
- **Betrieb und Steuerung** — Beschreibung des Steuercontroller-Menüs und der Arbeitsmodi
- **FAQ** — Häufig gestellte Fragen zu iDryer und iHeater

## Schnellstart

Beginnen Sie mit einer allgemeinen Vertrautheit mit der Dokumentation und arbeiten Sie sich durch das Menü von oben nach unten vor. Bereiten Sie zunächst die Hardware vor und stellen Sie sicher, dass Sie alle erforderlichen Module, Kabel und die Stromversorgung haben. Programmieren Sie dann die Anfangsfirmware des Steuercontrollers, konfigurieren Sie die Anschlüsse für Ihre Konfiguration und verbinden Sie die Hardware.

Der Hauptworkflow wird derzeit um das Portal herum aufgebaut: Programmieren Sie den Steuercontroller, verbinden Sie dann iDryer-Link und programmieren Sie es, führen Sie die Wi-Fi-Verbindung durch, Claim und koppeln Sie das Gerät mit dem Portal. Danach können Sie den Trockner über das Portal oder die App verwenden, und das Display bleibt eine bequeme Service-Methode zur Steuerung und Konfiguration, wenn Sie das Gerät lokal betreiben möchten.

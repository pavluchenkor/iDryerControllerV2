# Rychlý start

Krátká cesta od tisku dílů k funkční sušičce: tisk krytu, zapojení modulů,
nahrání firmwaru, spárování s portálem (claim) a prvotní nastavení klapky.
Podrobnosti ke každému kroku najdete v příslušných částech dokumentace.

## Co budete potřebovat

- Řídicí desku (RP2040) a modul **iDryer-Link** (ESP32-C3 Super Mini).
- Datový USB kabel.
- Prohlížeč na bázi Chromium (Chrome, Edge) s podporou WebUSB.
- Síť Wi-Fi 2,4 GHz s heslem.
- Účet na portálu — <https://portal.idryer.org>.

## 1. Zapojení

!!! warning "Nejprve sestavu vyzkoušejte na stole"
    Před finální montáží složte všechny komponenty na stole a ověřte, že
    zařízení funguje. Chyby zapojení se hledají snáze, dokud máte přístup ke
    všem dílům.

!!! danger "Napájení"
    Nikdy nepřipojujte ani neodpojujte moduly (Link, displej, senzory) pod
    napětím. Veškeré zapojení provádějte při vypnutém napájení.

Zapojte moduly a komponenty podle příslušné části dokumentace. Link musí být
připojen k řídicí desce ještě před nahráním firmwaru.

!!! warning "Nezaměňte vodiče"
    Zapojení vypadá jednoduše, ale vodiče k desce se často zamění. Takové chyby
    se na dálku diagnostikují obtížně: sušička navenek pracuje normálně, ale
    mění se řídicí logika. Například při záměně ventilátoru a topení zařízení
    zdánlivě funguje, ale PID regulátor topení neřídí — topení běží trvale na
    plný výkon.

## 2. Nahrání firmwaru do řídicí desky a Linku

Firmware se nahrává z prohlížeče na <https://install.idryer.org>. Postupujte
kroky průvodce v pořadí:

1. **Flash Controller** — připojte USB k portu řídicí desky, přepněte desku do
   režimu `BOOTSEL` a nahrajte firmware do řídicí desky.
2. **Flash Link** — přepojte USB kabel na port Link (Link zůstává připojen k
   řídicí desce) a nahrajte firmware do modulu.

## 3. Wi-Fi a spárování s portálem

Pokračujte ve stejném průvodci na <https://install.idryer.org>:

1. **Wi-Fi** — po nahrání firmwaru do Linku se otevře průvodce nastavením sítě
   (Improv). Zadejte název (SSID) a heslo své sítě Wi-Fi.
2. **Claim** — spusťte spárování. Průvodce zobrazí `PIN`.
3. **Portál** — otevřete <https://portal.idryer.org>, přihlaste se, na stránce
   zařízení přidejte zařízení a zadejte `PIN`.

Po spárování se zařízení objeví v seznamu na portálu.

## 4. Tisk dílů krytu

Díly krytu tiskněte s parametry uvedenými v části CAD dokumentace. Tyto
parametry jsou ověřené tisíci sestav. Při odchylce od nich kryt ztrácí tepelnou
izolaci a sušička nedosáhne provozní teploty.

## 5. Klapka a servo

Klapku lze nastavit z displeje řídicí desky (menu `SETTINGS → SERVO`), z
nastavení zařízení na portálu nebo z aplikace.

!!! warning "Pořadí montáže klapky"
    Nejprve nastavte úhel a teprve poté klapku namontujte — jinak narazí do
    krytu a zablokuje servo.

1. Nastavte `CLOSED ANGLE = 0`. Servo se přesune do této polohy (náhled).
2. Podle skutečné polohy hřídele namontujte klapku tak, aby v zavřené poloze
   zcela uzavírala <!-- TODO: ověřit termín — otvor/kanál sestavy klapky -->
   vzduchový kanál sestavy klapky.
3. Nastavte `OPEN ANGLE` podle své mechaniky. Tento krok lze provést i po
   finální montáži.

## 6. PID regulátor topení

Firmware již obsahuje funkční hodnoty PID regulátoru — pro spuštění a prvotní
kontrolu není samostatná kalibrace nutná. V případě potřeby spusťte autotune,
abyste koeficienty přizpůsobili své sestavě.

## 7. Ovládání přes portál a aplikaci

Všechny funkce a nabídky řídicí desky jsou dostupné přes portál a aplikaci.
Portál a aplikace výrazně rozšiřují možnosti sušičky: telemetrie, historie dat,
předvolby a vzdálené ovládání.

Ovládání je dostupné z portálu <https://portal.idryer.org> nebo z aplikace:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

Spuštění sušení:

1. Otevřete portál nebo aplikaci — na obrazovce se zobrazí dlaždice vašeho
   zařízení.
2. Vyberte režim — sušení nebo skladování.
3. Stiskněte start.

Výchozí hodnoty teploty a času jsou zvolené pro většinu případů. V případě
potřeby je upravte podle svého materiálu.

### Evidence filamentu a recenze

Každý filament na vaší polici se odráží na portálu a všechna data se
zaznamenávají. Ke každému filamentu můžete napsat recenzi a číst recenze
ostatních uživatelů. Recenze jsou seskupené podle výrobce, typu a dalších
atributů a jsou dostupné přímo na portálu a na fóru.

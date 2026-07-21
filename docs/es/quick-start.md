# Inicio rápido

Un recorrido corto desde la impresión de las piezas hasta un secador en
funcionamiento: imprimir la carcasa, cablear los módulos, flashear, vincular en
el portal (claim) y ajustar por primera vez la compuerta. Los detalles de cada
paso están en las secciones dedicadas de la documentación.

## Qué necesita

- Placa controladora (RP2040) y el módulo **iDryer-Link** (ESP32-C3 Super Mini).
- Un cable USB de datos.
- Un navegador Chromium (Chrome, Edge) con soporte WebUSB.
- Una red Wi-Fi de 2,4 GHz con contraseña.
- Una cuenta en el portal — <https://portal.idryer.org>.

## 1. Cableado

!!! warning "Pruebe primero el montaje en la mesa"
    Antes del montaje final, reúna todos los componentes sobre la mesa y
    asegúrese de que el dispositivo funciona. Los errores de cableado son más
    fáciles de encontrar mientras aún tiene acceso a todas las piezas.

!!! danger "Alimentación"
    Nunca conecte ni desconecte módulos (Link, pantalla, sensores) con la
    alimentación aplicada. Realice todo el cableado con el dispositivo apagado.

Cablee los módulos y componentes según la sección correspondiente de la
documentación. Link debe estar conectado al controlador antes de flashear.

!!! warning "No intercambie los cables"
    El cableado parece sencillo, pero los cables a la placa se intercambian a
    menudo. Estos errores son difíciles de diagnosticar de forma remota: el
    secador parece funcionar con normalidad, pero la lógica de control cambia.
    Por ejemplo, si se intercambian el ventilador y el calentador, el
    dispositivo parece funcionar, pero el regulador PID no controla el
    calentador — el calentador funciona todo el tiempo a plena potencia.

## 2. Flashear el controlador y Link

El flasheo se realiza desde un navegador en <https://install.idryer.org>. Siga
los pasos del asistente en orden:

1. **Flash Controller** — conecte el USB al puerto del controlador, ponga la
   placa en modo `BOOTSEL` y flashee el controlador.
2. **Flash Link** — pase el cable USB al puerto de Link (Link permanece
   conectado al controlador) y flashee el módulo.

## 3. Wi-Fi y vinculación en el portal

Continúe en el mismo asistente en <https://install.idryer.org>:

1. **Wi-Fi** — tras flashear Link se abre el asistente de configuración de red
   (Improv). Introduzca el nombre (SSID) y la contraseña de su red Wi-Fi.
2. **Claim** — inicie la vinculación. El asistente muestra un `PIN`.
3. **Portal** — abra <https://portal.idryer.org>, inicie sesión, añada un
   dispositivo en la página de dispositivos e introduzca el `PIN`.

Tras la vinculación, el dispositivo aparece en la lista del portal.

## 4. Impresión de las piezas de la carcasa

Imprima las piezas de la carcasa con los parámetros indicados en la sección CAD
de la documentación. Estos parámetros se han probado en miles de montajes. Si se
desvía de ellos, la carcasa pierde aislamiento térmico y el secador no alcanza
su temperatura de trabajo.

## 5. Compuerta y servomotor

Puede configurar la compuerta desde la pantalla del controlador (menú `SETTINGS →
SERVO`), desde los ajustes del dispositivo en el portal o desde la aplicación.

!!! warning "Orden de instalación de la compuerta"
    Ajuste primero el ángulo y solo después instale la compuerta — de lo
    contrario chocará contra la carcasa y bloqueará el servomotor.

1. Ajuste `CLOSED ANGLE = 0`. El servomotor se mueve a esta posición (vista
   previa).
2. Según la posición real del eje, instale la compuerta de modo que en la
   posición cerrada obture por completo <!-- TODO: confirmar término —
   abertura/conducto del conjunto de la compuerta --> el canal de aire del
   conjunto de la compuerta.
3. Ajuste `OPEN ANGLE` según su mecánica. Este paso también puede realizarse
   después del montaje final.

## 6. Regulador PID del calentador

El firmware ya incluye valores de regulador PID funcionales — no se requiere una
calibración aparte para arrancar y realizar la comprobación inicial. Si es
necesario, ejecute el autotune para ajustar los coeficientes a su montaje.

## 7. Control mediante el portal y la aplicación

Todas las funciones y menús del controlador están disponibles a través del
portal y la aplicación. El portal y la aplicación amplían notablemente las
posibilidades del secador: telemetría, historial de datos, preajustes y control
remoto.

El control está disponible desde el portal <https://portal.idryer.org> o desde
la aplicación:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

Para iniciar el secado:

1. Abra el portal o la aplicación — la ficha de su dispositivo aparece en la
   pantalla.
2. Seleccione el modo — secado o almacenamiento.
3. Pulse iniciar.

Los valores de temperatura y tiempo predeterminados están elegidos para la
mayoría de los casos. Cámbielos según su material si es necesario.

### Registro de filamento y reseñas

Cada filamento de su estante se refleja en el portal, y todos los datos quedan
registrados. Puede dejar una reseña de cada filamento y leer las reseñas de
otros usuarios. Las reseñas se agrupan por fabricante, tipo y otros atributos y
están disponibles directamente en el portal y en el foro.

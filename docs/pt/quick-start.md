# Início rápido

Um percurso curto, da impressão das peças a um secador a funcionar: imprimir a
caixa, cablar os módulos, fazer o flash, associar no portal (claim) e a
configuração inicial da comporta. Os detalhes de cada passo estão nas secções
dedicadas da documentação.

## Do que precisa

- Placa controladora (RP2040) e o módulo **iDryer-Link** (ESP32-C3 Super Mini).
- Um cabo USB de dados.
- Um navegador Chromium (Chrome, Edge) com suporte a WebUSB.
- Uma rede Wi-Fi de 2,4 GHz com palavra-passe.
- Uma conta no portal — <https://portal.idryer.org>.

## 1. Cablagem

!!! warning "Teste primeiro a montagem na bancada"
    Antes da montagem final, junte todos os componentes na bancada e confirme
    que o dispositivo funciona. Os erros de cablagem são mais fáceis de
    encontrar enquanto ainda tem acesso a todas as peças.

!!! danger "Alimentação"
    Nunca ligue nem desligue módulos (Link, ecrã, sensores) com a alimentação
    ligada. Faça toda a cablagem com o dispositivo desligado.

Cable os módulos e componentes de acordo com a secção correspondente da
documentação. O Link tem de estar ligado ao controlador antes do flash.

!!! warning "Não troque os fios"
    A cablagem parece simples, mas os fios para a placa são frequentemente
    trocados. Esses erros são difíceis de diagnosticar à distância: o secador
    parece funcionar normalmente, mas a lógica de controlo muda. Por exemplo, se
    a ventoinha e o aquecedor forem trocados, o dispositivo parece funcionar,
    mas o controlador PID não controla o aquecedor — o aquecedor trabalha
    sempre à potência máxima.

## 2. Flash do controlador e do Link

O flash é feito a partir de um navegador em <https://install.idryer.org>. Siga os
passos do assistente pela ordem:

1. **Flash Controller** — ligue o USB à porta do controlador, coloque a placa em
   modo `BOOTSEL` e faça o flash do controlador.
2. **Flash Link** — passe o cabo USB para a porta do Link (o Link permanece
   ligado ao controlador) e faça o flash do módulo.

## 3. Wi-Fi e associação no portal

Continue no mesmo assistente em <https://install.idryer.org>:

1. **Wi-Fi** — após o flash do Link, abre-se o assistente de configuração de
   rede (Improv). Introduza o nome (SSID) e a palavra-passe da sua rede Wi-Fi.
2. **Claim** — inicie a associação. O assistente mostra um `PIN`.
3. **Portal** — abra <https://portal.idryer.org>, inicie sessão, adicione um
   dispositivo na página de dispositivos e introduza o `PIN`.

Após a associação, o dispositivo aparece na lista do portal.

## 4. Impressão das peças da caixa

Imprima as peças da caixa com os parâmetros indicados na secção CAD da
documentação. Estes parâmetros foram comprovados em milhares de montagens. Se se
desviar deles, a caixa perde o isolamento térmico e o secador não atinge a sua
temperatura de funcionamento.

## 5. Comporta e servomotor

Pode configurar a comporta a partir do ecrã do controlador (menu `SETTINGS →
SERVO`), a partir das definições do dispositivo no portal ou a partir da
aplicação.

!!! warning "Ordem de instalação da comporta"
    Defina primeiro o ângulo e só depois instale a comporta — caso contrário,
    esta bate na caixa e bloqueia o servomotor.

1. Defina `CLOSED ANGLE = 0`. O servomotor desloca-se para esta posição
   (pré-visualização).
2. Consoante a posição real do veio, instale a comporta de modo que, na posição
   fechada, obture por completo <!-- TODO: confirmar o termo —
   abertura/conduta do conjunto da comporta --> o canal de ar do conjunto da
   comporta.
3. Defina `OPEN ANGLE` conforme a sua mecânica. Este passo também pode ser feito
   após a montagem final.

## 6. Controlador PID do aquecedor

O firmware já inclui valores de controlador PID funcionais — não é necessária uma
calibração separada para arrancar e fazer a verificação inicial. Se necessário,
execute o autotune para ajustar os coeficientes à sua montagem.

## 7. Controlo pelo portal e pela aplicação

Todas as funções e menus do controlador estão disponíveis através do portal e da
aplicação. O portal e a aplicação ampliam significativamente as capacidades do
secador: telemetria, histórico de dados, predefinições e controlo remoto.

O controlo está disponível a partir do portal <https://portal.idryer.org> ou da
aplicação:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

Para iniciar a secagem:

1. Abra o portal ou a aplicação — o cartão do seu dispositivo aparece no ecrã.
2. Selecione o modo — secagem ou armazenamento.
3. Prima iniciar.

Os valores predefinidos de temperatura e tempo estão escolhidos para a maioria
dos casos. Altere-os conforme o seu material, se necessário.

### Registo de filamento e avaliações

Cada filamento na sua prateleira é refletido no portal, e todos os dados são
registados. Pode deixar uma avaliação de cada filamento e ler as avaliações de
outros utilizadores. As avaliações são agrupadas por fabricante, tipo e outros
atributos e estão disponíveis diretamente no portal e no fórum.

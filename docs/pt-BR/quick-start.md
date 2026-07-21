# Início rápido

Um percurso curto, da impressão das peças a um secador funcionando: imprimir a
caixa, fazer a fiação dos módulos, gravar o firmware, associar no portal (claim)
e a configuração inicial da comporta. Os detalhes de cada passo estão nas seções
dedicadas da documentação.

## Do que você precisa

- Placa controladora (RP2040) e o módulo **iDryer-Link** (ESP32-C3 Super Mini).
- Um cabo USB de dados.
- Um navegador Chromium (Chrome, Edge) com suporte a WebUSB.
- Uma rede Wi-Fi de 2,4 GHz com senha.
- Uma conta no portal — <https://portal.idryer.org>.

## 1. Fiação

!!! warning "Teste primeiro a montagem na bancada"
    Antes da montagem final, junte todos os componentes na bancada e confirme
    que o dispositivo funciona. Os erros de fiação são mais fáceis de encontrar
    enquanto você ainda tem acesso a todas as peças.

!!! danger "Alimentação"
    Nunca conecte nem desconecte módulos (Link, tela, sensores) com a
    alimentação ligada. Faça toda a fiação com o dispositivo desligado.

Faça a fiação dos módulos e componentes de acordo com a seção correspondente da
documentação. O Link deve estar conectado ao controlador antes da gravação.

!!! warning "Não troque os fios"
    A fiação parece simples, mas os fios para a placa são frequentemente
    trocados. Esses erros são difíceis de diagnosticar remotamente: o secador
    parece funcionar normalmente, mas a lógica de controle muda. Por exemplo, se
    o ventilador e o aquecedor forem trocados, o dispositivo parece funcionar,
    mas o controlador PID não controla o aquecedor — o aquecedor trabalha o
    tempo todo em potência máxima.

## 2. Gravação do controlador e do Link

A gravação é feita a partir de um navegador em <https://install.idryer.org>. Siga
os passos do assistente em ordem:

1. **Flash Controller** — conecte o USB à porta do controlador, coloque a placa
   em modo `BOOTSEL` e grave o controlador.
2. **Flash Link** — passe o cabo USB para a porta do Link (o Link permanece
   conectado ao controlador) e grave o módulo.

## 3. Wi-Fi e associação no portal

Continue no mesmo assistente em <https://install.idryer.org>:

1. **Wi-Fi** — após a gravação do Link, abre-se o assistente de configuração de
   rede (Improv). Informe o nome (SSID) e a senha da sua rede Wi-Fi.
2. **Claim** — inicie a associação. O assistente exibe um `PIN`.
3. **Portal** — abra <https://portal.idryer.org>, faça login, adicione um
   dispositivo na página de dispositivos e informe o `PIN`.

Após a associação, o dispositivo aparece na lista do portal.

## 4. Impressão das peças da caixa

Imprima as peças da caixa com os parâmetros indicados na seção CAD da
documentação. Esses parâmetros foram comprovados em milhares de montagens. Se
você se desviar deles, a caixa perde o isolamento térmico e o secador não
atinge sua temperatura de trabalho.

## 5. Comporta e servomotor

Você pode configurar a comporta pela tela do controlador (menu `SETTINGS →
SERVO`), pelas configurações do dispositivo no portal ou pelo aplicativo.

!!! warning "Ordem de instalação da comporta"
    Defina primeiro o ângulo e só depois instale a comporta — caso contrário,
    ela bate na caixa e trava o servomotor.

1. Defina `CLOSED ANGLE = 0`. O servomotor se move para esta posição
   (pré-visualização).
2. Conforme a posição real do eixo, instale a comporta de modo que, na posição
   fechada, ela obstrua por completo <!-- TODO: confirmar o termo —
   abertura/duto do conjunto da comporta --> o canal de ar do conjunto da
   comporta.
3. Defina `OPEN ANGLE` conforme a sua mecânica. Este passo também pode ser feito
   após a montagem final.

## 6. Controlador PID do aquecedor

O firmware já vem com valores de controlador PID funcionais — não é necessária
uma calibração separada para iniciar e fazer a verificação inicial. Se
necessário, execute o autotune para ajustar os coeficientes à sua montagem.

## 7. Controle pelo portal e pelo aplicativo

Todas as funções e menus do controlador estão disponíveis pelo portal e pelo
aplicativo. O portal e o aplicativo ampliam significativamente os recursos do
secador: telemetria, histórico de dados, predefinições e controle remoto.

O controle está disponível pelo portal <https://portal.idryer.org> ou pelo
aplicativo:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

Para iniciar a secagem:

1. Abra o portal ou o aplicativo — o cartão do seu dispositivo aparece na tela.
2. Selecione o modo — secagem ou armazenamento.
3. Pressione iniciar.

Os valores padrão de temperatura e tempo estão escolhidos para a maioria dos
casos. Altere-os conforme o seu material, se necessário.

### Registro de filamento e avaliações

Cada filamento na sua prateleira é refletido no portal, e todos os dados são
registrados. Você pode deixar uma avaliação de cada filamento e ler as
avaliações de outros usuários. As avaliações são agrupadas por fabricante, tipo
e outros atributos e ficam disponíveis diretamente no portal e no fórum.

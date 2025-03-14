# 📌 Babá Eletrônica com Raspberry Pi Pico W

## 📖 Visão Geral do Projeto

### 🎯 Objetivo
Criar um protótipo de babá eletrônica que, ao detectar ruídos (como o choro de um bebê), toca uma música de ninar para acalmar o ambiente. O sistema pode ser habilitado ou desabilitado por meio de botões físicos (localmente) ou por meio de uma interface web, permitindo controle remoto.

### 🔹 Funcionalidades Principais
- **Detecção de som:** Utiliza o ADC para ler a entrada de um microfone e detectar ruídos com base em um limiar pré-definido.
- **Reprodução de música:** Ao detectar som, o sistema toca uma melodia pré-definida (música de ninar) por meio de um buzzer controlado via PWM.
- **Controle local:** Dois botões possibilitam ativar ou desativar o sistema.
- **Controle remoto via Webserver:** Um servidor web incorporado permite controlar o sistema por meio de requisições HTTP.
- **Feedback visual:** Um display OLED (SSD1306) e LEDs indicam o estado do sistema e notificam a ocorrência de som.

---

## 🛠️ Hardware Necessário

- **Placa Pi Pico W:** Microcontrolador com conectividade Wi‑Fi.
- **Microfone (ADC):** Conectado ao **GPIO 28** para detecção de som.
- **Buzzer (alto-falante):** Conectado ao **GPIO 21** e acionado via PWM.
- **Display OLED (SSD1306):** Conectado via I2C (**SDA no GPIO 14 e SCL no GPIO 15**) para exibir mensagens e status.
- **Botões físicos:**  
  - **Botão A (GPIO 5):** Ativa o sistema.
  - **Botão B (GPIO 6):** Desativa o sistema e interrompe a reprodução da melodia.
- **LEDs de status:**  
  - **GPIO 13:** LED vermelho.
  - **GPIO 11:** LED verde.
  - **GPIO 12:** LED azul.
  
  Estes LEDs indicam visualmente o estado do sistema (ativo com detecção de som, ativo sem detecção ou desativado).

---

## ⚙️ Descrição do Funcionamento

### 🔹 Inicialização do Hardware
- Inicializa o ADC para leitura do microfone (**GPIO 28**).
- Configura o PWM para o buzzer (**GPIO 21**) e inicializa o display OLED via I2C.
- Configura os botões e os LEDs para entrada/saída.
- Exibe mensagens iniciais no display (*“Babá Eletrônica”, “Aguardando ativação…”*).

### 🌐 Conexão Wi‑Fi
- Utiliza a biblioteca `cyw43_arch` para inicializar a interface Wi‑Fi, conectando-se à rede definida.
- Após a conexão, exibe o endereço IP obtido no monitor serial e inicia o webserver.

### 🖥️ Webserver
- O webserver é iniciado na porta 80 e responde a requisições HTTP.
- **Rotas definidas:**
  - `GET /system/on`: Ativa o sistema.
  - `GET /system/off`: Desativa o sistema e interrompe a reprodução da melodia.
- Responde com uma página HTML contendo botões para controle remoto.

### 📊 Monitoramento e Ação
- No loop principal, o sistema verifica:
  - Se os botões físicos foram pressionados para alterar o estado.
  - Se o estado do sistema foi modificado, atualiza os LEDs e o display.
  - Realiza a leitura do ADC para detectar variações de som.
    - O cálculo é feito convertendo o valor do ADC em tensão, comparando a diferença com um valor de offset (`SOUND_OFFSET`) e um limiar (`SOUND_THRESHOLD`).
  - Se um som for detectado e o sistema estiver ativo, a função `play_melody()` é chamada para tocar a música de ninar.

### 🎵 Reprodução da Música
- A função `play_tone()` utiliza o PWM para gerar uma frequência específica no buzzer durante um período determinado.
- A função `play_melody()` percorre um array de notas (definido em *song.h*) e reproduz cada nota com sua respectiva duração, podendo ser interrompida via botão ou comando remoto.

---

## 🔍 Arquitetura de Software e Fluxo do Código

### 📂 Inclusão de Bibliotecas e Definições Iniciais
- **Bibliotecas Padrão e do Pico SDK:** Utilizadas para operações básicas (`stdio`, `string`, `math`) e acesso ao hardware (`I2C`, `ADC`, `PWM`, `clocks`, etc.).
- **Bibliotecas Específicas:**  
  - `ssd1306.h`: Driver para o display OLED.
  - `cyw43_arch.h`: Gerenciamento da interface Wi‑Fi.
  - `lwip/tcp.h`: Implementação do servidor TCP/IP.
  - `song.h`: Contém as definições da melodia (arrays com notas e durações).

### 🛠️ Inicialização de Módulos
- **ADC:** Inicializa o ADC e configura o pino do microfone, ajustando o canal de entrada (`adc_select_input`).
- **PWM para o Buzzer:** A função `pwm_init_buzzer()` configura o **GPIO 21** para funcionar com PWM, definindo o clock divisor e iniciando o PWM.
- **Display OLED:** Inicializa o display via I2C, desenha mensagens iniciais e configura a área de renderização.
- **Botões e LEDs:** Configura os pinos dos botões como entrada com pull-up e os LEDs como saída. A função `update_led_status()` atualiza os LEDs conforme o estado do sistema e se um som foi detectado.
- **Wi‑Fi:** Utiliza a biblioteca `CYW43` para configurar e conectar à rede Wi‑Fi. Em caso de sucesso, exibe o IP e inicia o webserver.
- **Webserver:** Configura um servidor TCP que responde a requisições HTTP. As funções `http_callback()` e `connection_callback()` interpretam os comandos e atualizam as variáveis de estado (`system_active` e `melody_active`).

---

## 📌 Considerações Finais

### 🔧 Ajuste de Parâmetros
- Os valores de **`SOUND_OFFSET`** e **`SOUND_THRESHOLD`** podem ser calibrados de acordo com o ambiente e o sensor utilizado.
- As durações das notas e a melodia (definidas em *song.h*) podem ser modificadas para qualquer música de ninar.

### 💡 Feedback Visual e Controle Remoto
- O display OLED e os LEDs fornecem um feedback visual útil para monitorar o estado do sistema.
- O webserver permite um controle remoto simples, acessível via navegador.

### 🚀 Expansibilidade
O protótipo pode ser expandido para incluir funcionalidades adicionais, como:
- Notificações via rede.
- Armazenamento de logs.
- Ajustes dinâmicos de parâmetros.

---

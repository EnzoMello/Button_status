# 🌡️ Button + Temperature Monitor - RP2040 + Wi-Fi (Pico W)

Este projeto utiliza o microcontrolador **RP2040 (BitDogLab/Pico W)** para monitorar o **estado de dois botões físicos (A e B)** e a **temperatura ambiente** (via sensor analógico TMP36 no GPIO28). As informações são enviadas via **rede Wi-Fi** para um servidor HTTP embutido, onde uma **interface web exibe os dados em tempo real**.

---

## ⚙️ Tecnologias Utilizadas

- **C com Pico SDK** – Programação do firmware para RP2040  
- **Wi-Fi com CYW43** – Comunicação sem fio  
- **lwIP TCP Stack** – Servidor TCP embutido na própria placa  
- **HTML** – Interface web para visualização dos dados  
- **JSON** – Comunicação de dados entre cliente e servidor

---

## 🧠 Funcionalidades

- Leitura dos botões A e B conectados aos pinos GPIO 5 e 6  
- Leitura da temperatura via sensor analógico no GPIO 28 (canal ADC2)  
- Cálculo da média de 10 leituras de tensão para suavizar a leitura da temperatura  
- Servidor HTTP embarcado na porta 80  
- Endpoint `/` que serve uma página HTML com exibição em tempo real dos dados  
- Endpoint `/data` que retorna um JSON com os estados dos botões e temperatura  
- Atualização automática a cada segundo via `fetch()` no JavaScript

---

## 🧪 Estrutura do JSON (GET /data)

```json
{
  "temperatura": 25.63,
  "btn_a": 1,
  "btn_b": 0
}

```
# Clone o repositório
git clone https://github.com/seu-usuario/seu-repositorio.git

# Acesse o diretório do projeto
cd seu-repositorio

# Compile o projeto (certifique-se de ter o ambiente com Pico SDK configurado)
```json
mkdir build
cd build
cmake ..
make
```

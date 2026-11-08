Tarea 3: Búsqueda del Tesoro en dos Arduino UNO

---

## 1. Descripción del Proyecto
El proyecto consiste en un sistema embebido distribuido basado en dos placas Arduino UNO que se comunican mediante UART utilizando `SoftwareSerial` a una velocidad de 9600 baudios.

El juego sitúa un tesoro virtual de $2 \times 2$ celdas dentro de una grilla de $100 \times 100$ posiciones (coordenadas 0 a 100 en cada eje).
* **Módulo Esclavo (Joystick):** Interfaz física del usuario. Lee y filtra analógicamente dos potenciómetros (ejes X e Y), detecta la pulsación de un botón de inicio mediante debouncing no bloqueante y proyecta el nivel de cercanía en dos LEDs individuales mediante modulación PWM.
* **Módulo Maestro (Servidor de Juego):** Gobierna la lógica principal a través de una máquina de estados finitos (`enum`/`switch-case`). Sortea aleatoriamente la ubicación del tesoro en $[12, 100]$, calcula las distancias absolutas a la celda más cercana, evalúa la condición de permanencia para la victoria o el tiempo límite para la derrota, modula un LED verde indicador de tiempo y despliega el estado del juego, coordenadas y pistas en una pantalla LCD 16x2 mediante bus I2C.

---

## 2. Diagrama de la Máquina de Estados del Maestro

     ```text
                     +---------------------------------------+
                     |               INACTIVO                | <--------------------------+
                     |  - LED Verde apagado                  |                            |
                     |  - LCD: "Pulsa Inicio"                |                            |
                     +---------------------------------------+                            |
                                         |                                                |
                                         | Pulsador Esclavo (Byte 0xFF)                   |
                                         v                                                |
                     +---------------------------------------+                            |
         +----------------> |                JUGANDO                |                     |
         |                  |  - Temporizador activo (60 s)         |                     |
         |                  |  - Atenuacion PWM de LED Verde        |                     |
         |                  |  - Mapeo de niveles (Frio -> Aqui)    |                     |
         |                  |  - Despliegue de datos en LCD I2C     |                     |
         |                  +---------------------------------------+                     |
         |                                |                   |                           |
       dx=0 y dy=0                        |                   | Tiempo >= 60 s            |
       continuo por 2000 ms               v                   v                           |
                     +-------------------+   +-------------------+                        |
                     |     VICTORIA      |   |      DERROTA      |                        |
                     | - LED Verde 100%  |   | - LED Verde titila|                        |
                     | - LEDs Esclavo:   |   |   (250 ms)        |                        |
                     |   parpadeo x10    |   | - LEDs Esclavo:   |                        |
                     |   (250 ms)        |   |   apagados        |                        |
                     +-------------------+   +-------------------+                        |
                               |                       |                                  |
                               +-----------+-----------+                                  |
                                           |                                              |
                                           | Animacion finalizada (5 segundos)            |
                                           +----------------------------------------------+
Protocolo Serial Binario (1 Byte) y Tabla de Niveles

```markdown
---

## 3. Protocolo de Comunicación Serial (1 Byte Binario)

La comunicación serial es punto a punto, bidireccional y periódica (cada 40–50 ms). Cumple estrictamente con el requisito de transmisión binaria mediante `write()` y recepción no bloqueante con `read()`, descartando el uso de cadenas ASCII (`print()`).

### Desambiguación de Bytes
Para evitar colisiones entre coordenadas y comandos sin agregar bytes de cabecera, se aplica un protocolo basado en rangos y máscaras de bits:

| Dirección | Byte / Rango (Hex) | Rango Decimal | Significado / Carga Útil | Momento de Envío |
| :--- | :--- | :--- | :--- | :--- |
| **Esclavo $\rightarrow$ Maestro** | `0x00` a `0x64` | `0` a `100` | **Coordenada Eje X** | Periódico (cada 40 ms) |
| **Esclavo $\rightarrow$ Maestro** | `0x80` a `0xE4` | `128` a `228` | **Coordenada Eje Y** (valor $Y + 128$) | Periódico (cada 40 ms) |
| **Esclavo $\rightarrow$ Maestro** | `0xFF` | `255` | **Solicitud de Inicio de Partida** | Un único envío por pulsación |
| **Maestro $\rightarrow$ Esclavo** | `0x00` | `0` | **Estado INACTIVO** | Periódico en reposo |
| **Maestro $\rightarrow$ Esclavo** | `0x01` | `1` | **Estado VICTORIA** | Animación de acierto |
| **Maestro $\rightarrow$ Esclavo** | `0x02` | `2` | **Estado DERROTA** | Animación de tiempo agotado |
| **Maestro $\rightarrow$ Esclavo** | `0x10` a `0x1F` | `16` a `31` | **Estado JUGANDO** (Niveles X e Y empaquetados) | Periódico durante partida |

### Estructura del Byte de Juego (`0x10` a `0x1F`):
```text
  Bit 7   Bit 6   Bit 5   Bit 4   Bit 3   Bit 2   Bit 1   Bit 0
+-------+-------+-------+-------+-------+-------+-------+-------+
|   0   |   0   |   0   |   1   |    Nivel X    |    Nivel Y    |
+-------+-------+-------+-------+-------+-------+-------+-------+
                                  \_____ _____/   \_____ _____/
                                        v               v
                                   2 bits (0-3)    2 bits (0-3)
Esquema de Pines, Decisiones Técnicas y Declaración de IA

```markdown
---

## 4. Diagrama Esquemático y Asignación de Pines

```text
           +---------------------------------------+
           |           ARDUINO ESCLAVO             |
           |              (JOYSTICK)               |
           +---------------------------------------+
             | A0         <--- Pin Central Potenciómetro X (0-100)
             | A1         <--- Pin Central Potenciómetro Y (0-100)
             | Pin 2      <--- Pulsador de Inicio (INPUT_PULLUP a GND)
             | Pin 5(PWM) ---> Ánodo (+) LED X [R = 220-330 Ω] ---> Cátodo (-) GND
             | Pin 6(PWM) ---> Ánodo (+) LED Y [R = 220-330 Ω] ---> Cátodo (-) GND
             | Pin 10(RX) <=============================================+
             | Pin 11(TX) =========================================+    |
             | GND        ====================================+    |    |
           +---------------------------------------+          |    |    |
                                                              |    |    |
           +---------------------------------------+          |    |    |
           |           ARDUINO MAESTRO             |          |    |    |
           |          (LOGICA JUEGO + LCD)         |          |    |    |
           +---------------------------------------+          |    |    |
             | GND        ====================================+    |    |
             | Pin 10(RX) <========================================+    |
             | Pin 11(TX) ==============================================+
             | Pin 9(PWM) ---> Ánodo (+) LED Verde (Timer) [R = 220 Ω] -> GND
             | A4 (SDA)   ---> Pin SDA de Pantalla LCD 16x2 I2C
             | A5 (SCL)   ---> Pin SCL de Pantalla LCD 16x2 I2C
             | 5V / GND   ---> VCC / GND del Módulo I2C LCD
             | A0 (Libre) ---> Ruido analógico al aire para randomSeed()
           +---------------------------------------+

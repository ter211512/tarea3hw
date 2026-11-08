Tarea 3: Búsqueda del Tesoro en dos Arduino UNO

---

## 1. Descripción del Proyecto
El proyecto consiste en un sistema embebido distribuido basado en dos placas Arduino UNO que se comunican mediante UART utilizando `SoftwareSerial` a una velocidad de 9600 baudios.

El juego sitúa un tesoro virtual de $2 \times 2$ celdas dentro de una grilla de $100 \times 100$ posiciones (coordenadas 0 a 100 en cada eje).
* **Módulo Esclavo (Joystick):** Interfaz física del usuario. Lee y filtra analógicamente dos potenciómetros (ejes X e Y), detecta la pulsación de un botón de inicio mediante debouncing no bloqueante y proyecta el nivel de cercanía en dos LEDs individuales mediante modulación PWM.
* **Módulo Maestro (Servidor de Juego):** Gobierna la lógica principal a través de una máquina de estados finitos (`enum`/`switch-case`). Sortea aleatoriamente la ubicación del tesoro en $[12, 100]$, calcula las distancias absolutas a la celda más cercana, evalúa la condición de permanencia para la victoria o el tiempo límite para la derrota, modula un LED verde indicador de tiempo y despliega el estado del juego, coordenadas y pistas en una pantalla LCD 16x2 mediante bus I2C.

---

## 2. Diagrama de la Máquina de Estados del Maestro

       +---------------------------------------+
                     |               INACTIVO                | <--------------------------+
                     |  - LED Verde apagado                  |                            |
                     |  - LCD: "Pulsa Inicio"                |                            |
                     +---------------------------------------+                            |
                                         |                                                |
                                         | Pulsador Esclavo (Byte 0xFF)                   |
                                         v                                                |
                     +---------------------------------------+                            |
  +----------------> |                JUGANDO                |                            |
  |                  |  - Temporizador activo (60 s)         |                            |
  |                  |  - Atenuacion PWM de LED Verde        |                            |
  |                  |  - Mapeo de niveles (Frio -> Aqui)    |                            |
  |                  |  - Despliegue de datos en LCD I2C     |                            |
  |                  +---------------------------------------+                            |
  |                                |                   |                                  |
dx=0 y dy=0                        |                   | Tiempo >= 60 s                   |
continuo por 2000 ms               v                   v                                  |
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




                                         

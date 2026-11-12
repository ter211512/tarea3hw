/**
 * ============================================================================
 * CIN-212 Hardware Digital - Universidad de Valparaiso
 * Tarea 3: Modulo Maestro (Logica de Juego, Maquina de Estados y LCD I2C)
 * Integrantes: Walter Valencia / Lucas Marchant
 * ============================================================================
 */
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Definiciones
const uint8_t PIN_LED_VERDE     = 9; 
const uint8_t PIN_RX_LINK       = 10;
const uint8_t PIN_TX_LINK       = 11;
const uint8_t PIN_RUIDO_SEMILLA = A0;

SoftwareSerial enlaceEsclavo(PIN_RX_LINK, PIN_TX_LINK);
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Constantes
const unsigned long DURACION_PARTIDA_MS = 60000;
const unsigned long INTERVALO_ENVIO_MS  = 50;
const unsigned long TIMEOUT_ENLACE_MS   = 2000;
const unsigned long TIEMPO_VICTORIA_MS  = 2000;

enum EstadoJuego { INACTIVO, JUGANDO, VICTORIA, DERROTA };
EstadoJuego estadoActual = INACTIVO;

//Variables
uint8_t tesoroX = 50;
uint8_t tesoroY = 50;
uint8_t jugadorX = 0;
uint8_t jugadorY = 0;

uint8_t distanciaX = 0;
uint8_t distanciaY = 0;
uint8_t nivelX = 0;
uint8_t nivelY = 0;

// Temporizadores
unsigned long tiempoInicioPartida = 0;
unsigned long tiempoDentroTesoro = 0;
unsigned long ultimoEnvio = 0;
unsigned long ultimoMensajeRecibido = 0;
unsigned long tiempoEntradaEstadoFinal = 0;

bool enlacePerdido = false;

//Funciones
void inicializarSistema();
void procesarRecepcionEsclavo();
void gestionarEnlaceYTimeout();
void actualizarMaquinaEstados();
void transmitirEstadoEsclavo();
uint8_t calcularDistanciaEje(uint8_t coordJugador, uint8_t refTesoro);
uint8_t determinarNivel(uint8_t distancia);
void iniciarPartida();
void refrescarLcdInactivo();
void refrescarLcdJugando(int segundosRestantes);

void setup() {
  inicializarSistema();
}

void loop() {
  procesarRecepcionEsclavo();
  gestionarEnlaceYTimeout();

  if (!enlacePerdido) {
    actualizarMaquinaEstados();
  }

  
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = millis();
    transmitirEstadoEsclavo();
  }
}

void inicializarSistema() {
  pinMode(PIN_LED_VERDE, OUTPUT);
  analogWrite(PIN_LED_VERDE, 0);

  Serial.begin(9600);
  enlaceEsclavo.begin(9600);

  lcd.init();
  lcd.backlight();
  refrescarLcdInactivo();

  randomSeed(analogRead(PIN_RUIDO_SEMILLA));
  ultimoMensajeRecibido = millis();
}

void procesarRecepcionEsclavo() {
  while (enlaceEsclavo.available() > 0) {
    uint8_t byteIn = enlaceEsclavo.read();
    ultimoMensajeRecibido = millis();

    if (byteIn == 0xFF) {
      if (estadoActual == INACTIVO) {
        iniciarPartida();
      }
    } else if (byteIn <= 100) {
      jugadorX = byteIn;
    } else if (byteIn >= 128 && byteIn <= 228) {
      jugadorY = byteIn - 128;
    }
  }
}

void gestionarEnlaceYTimeout() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - ultimoMensajeRecibido > TIMEOUT_ENLACE_MS) {
    if (!enlacePerdido) {
      enlacePerdido = true;
      if (estadoActual == JUGANDO) {
        estadoActual = INACTIVO;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("ENLACE PERDIDO"));
      lcd.setCursor(0, 1);
      lcd.print(F("Esperando Esclavo"));
    }
    bool parpadeo = ((tiempoActual / 250) % 2) == 0;
    analogWrite(PIN_LED_VERDE, parpadeo ? 255 : 0);
  } else {
    if (enlacePerdido) {
      enlacePerdido = false;
      refrescarLcdInactivo();
    }
  }
}

void actualizarMaquinaEstados() {
  unsigned long tiempoActual = millis();

  switch (estadoActual) {
    case INACTIVO:
      analogWrite(PIN_LED_VERDE, 0);
      break;

    case JUGANDO: {
      unsigned long tiempoTranscurrido = tiempoActual - tiempoInicioPartida;
      if (tiempoTranscurrido >= DURACION_PARTIDA_MS) {
        estadoActual = DERROTA;
        tiempoEntradaEstadoFinal = tiempoActual;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("TIEMPO AGOTADO!"));
        lcd.setCursor(0, 1);
        lcd.print(F("DERROTA"));
        break;
      }

      int tiempoRestanteS = (DURACION_PARTIDA_MS - tiempoTranscurrido) / 1000;
      uint8_t brilloPwm = (uint8_t)(((DURACION_PARTIDA_MS - tiempoTranscurrido) * 255L) / DURACION_PARTIDA_MS);
      analogWrite(PIN_LED_VERDE, brilloPwm);

      distanciaX = calcularDistanciaEje(jugadorX, tesoroX);
      distanciaY = calcularDistanciaEje(jugadorY, tesoroY);

      nivelX = determinarNivel(distanciaX);
      nivelY = determinarNivel(distanciaY);

      if (distanciaX == 0 && distanciaY == 0) {
        if (tiempoDentroTesoro == 0) {
          tiempoDentroTesoro = tiempoActual;
        } else if (tiempoActual - tiempoDentroTesoro >= TIEMPO_VICTORIA_MS) {
          estadoActual = VICTORIA;
          tiempoEntradaEstadoFinal = tiempoActual;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("¡VICTORIA!"));
          lcd.setCursor(0, 1);
          lcd.print(F("Tesoro Hallado!"));
          break;
        }
      } else {
        tiempoDentroTesoro = 0;
      }

      refrescarLcdJugando(tiempoRestanteS);
      break;
    }

    case VICTORIA: {
      analogWrite(PIN_LED_VERDE, 255);
      if (tiempoActual - tiempoEntradaEstadoFinal >= 5000) {
        estadoActual = INACTIVO;
        refrescarLcdInactivo();
      }
      break;
    }

    case DERROTA: {
      bool parpadeo = ((tiempoActual - tiempoEntradaEstadoFinal) / 250) % 2;
      analogWrite(PIN_LED_VERDE, parpadeo ? 255 : 0);
      if (tiempoActual - tiempoEntradaEstadoFinal >= 5000) {
        estadoActual = INACTIVO;
        refrescarLcdInactivo();
      }
      break;
    }
  }
}

uint8_t calcularDistanciaEje(uint8_t coordJugador, uint8_t refTesoro) {
  uint8_t limiteInferior = refTesoro - 1;
  uint8_t limiteSuperior = refTesoro;

  if (coordJugador >= limiteInferior && coordJugador <= limiteSuperior) {
    return 0;
  } else if (coordJugador < limiteInferior) {
    return limiteInferior - coordJugador;
  } else {
    return coordJugador - limiteSuperior;
  }
}

uint8_t determinarNivel(uint8_t distancia) {
  if (distancia == 0) return 3; 
  if (distancia <= 5) return 2;  
  if (distancia <= 10) return 1; 
  return 0;                      
}

void transmitirEstadoEsclavo() {
  if (estadoActual == INACTIVO) {
    enlaceEsclavo.write((uint8_t)0x00);
  } else if (estadoActual == VICTORIA) {
    enlaceEsclavo.write((uint8_t)0x01);
  } else if (estadoActual == DERROTA) {
    enlaceEsclavo.write((uint8_t)0x02);
  } else if (estadoActual == JUGANDO) {
    uint8_t paquete = 0x10 | ((nivelX & 0x03) << 2) | (nivelY & 0x03);
    enlaceEsclavo.write(paquete);
  }
}

void iniciarPartida() {
  tesoroX = random(12, 101);
  tesoroY = random(12, 101);

  tiempoInicioPartida = millis();
  tiempoDentroTesoro = 0;
  estadoActual = JUGANDO;
  lcd.clear();
}

void refrescarLcdInactivo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("BUSQUEDA TESORO"));
  lcd.setCursor(0, 1);
  lcd.print(F("Pulsa Inicio"));
}

void refrescarLcdJugando(int segundosRestantes) {
  lcd.setCursor(0, 0);
  lcd.print(F("X:"));
  if (jugadorX < 10) lcd.print(F("0"));
  lcd.print(jugadorX);
  lcd.print(F(" Y:"));
  if (jugadorY < 10) lcd.print(F("0"));
  lcd.print(jugadorY);
  lcd.print(F(" T:"));
  if (segundosRestantes < 10) lcd.print(F(" "));
  lcd.print(segundosRestantes);
  lcd.print(F("s"));

  lcd.setCursor(0, 1);
  lcd.print(F("Dx:"));
  lcd.print(distanciaX);
  lcd.print(F(" Dy:"));
  lcd.print(distanciaY);
  lcd.print(F("   "));
}

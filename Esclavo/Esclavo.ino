/**
 * ============================================================================
 * CIN-212 Hardware Digital - Universidad de Valparaiso
 * Tarea 3: Modulo Esclavo (Joystick y Retroalimentacion Luminosa)
 * Integrantes: Walter Valencia / Lucas Marchant
 * ============================================================================
 */
#include <SoftwareSerial.h>

//Definiciones
const uint8_t PIN_POT_X      = A0;
const uint8_t PIN_POT_Y      = A1;
const uint8_t PIN_BTN_INICIO = 2;
const uint8_t PIN_LED_X      = 5;
const uint8_t PIN_LED_Y      = 6;
const uint8_t PIN_RX_LINK    = 10;
const uint8_t PIN_TX_LINK    = 11;

SoftwareSerial enlaceMaestro(PIN_RX_LINK, PIN_TX_LINK);

//Constantes
const unsigned long INTERVALO_ENVIO_MS = 40;
const unsigned long TIEMPO_ANTIRREBOTE = 40;
const unsigned long TIMEOUT_ENLACE_MS  = 2000;  

const uint8_t PWM_FRIO     = 0;
const uint8_t PWM_TIBIO    = 20;
const uint8_t PWM_CALIENTE = 80;
const uint8_t PWM_AQUI     = 255;

//Variables
uint8_t coordX = 0;
uint8_t coordY = 0;
uint8_t nivelX = 0;
uint8_t nivelY = 0;
char estadoJuego = 'I';

unsigned long ultimoEnvio = 0;
unsigned long ultimoMensajeRecibido = 0;
bool enlacePerdido = false;

int estadoAnteriorBtn = HIGH;
int estadoEstableBtn  = HIGH;
unsigned long tiempoUltimoRebote = 0;

//Funciones
void inicializarHardware();
void leerPulsadorInicio();
void muestrearPotenciometros();
uint8_t procesarCanalADC(uint8_t pinAnalogico);
void transmitirCoordenadas();
void recibirRespuestaMaestro();
void actualizarLeds();
void animarPerdidaEnlace();
uint8_t mapearPWM(uint8_t nivel);

void setup() {
  inicializarHardware();
}

void loop() {
  unsigned long tiempoActual = millis();
  leerPulsadorInicio();
  if (tiempoActual - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = tiempoActual;
    muestrearPotenciometros();
    transmitirCoordenadas();
  }
  
  recibirRespuestaMaestro();
  if (tiempoActual - ultimoMensajeRecibido > TIMEOUT_ENLACE_MS) {
    enlacePerdido = true;
    animarPerdidaEnlace();
  } else {
    enlacePerdido = false;
    actualizarLeds();
  }
}

void inicializarHardware() {
  pinMode(PIN_BTN_INICIO, INPUT_PULLUP);
  pinMode(PIN_LED_X, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);

  analogWrite(PIN_LED_X, 0);
  analogWrite(PIN_LED_Y, 0);

  Serial.begin(9600);
  enlaceMaestro.begin(9600);
  ultimoMensajeRecibido = millis();
}

void leerPulsadorInicio() {
  int lectura = digitalRead(PIN_BTN_INICIO);

  if (lectura != estadoAnteriorBtn) {
    tiempoUltimoRebote = millis();
  }

  if ((millis() - tiempoUltimoRebote) > TIEMPO_ANTIRREBOTE) {
    if (lectura != estadoEstableBtn) {
      estadoEstableBtn = lectura;
      if (estadoEstableBtn == LOW) {
        enlaceMaestro.write((uint8_t)0xFF);
      }
    }
  }
  estadoAnteriorBtn = lectura;
}

uint8_t procesarCanalADC(uint8_t pinAnalogico) {
  analogRead(pinAnalogico);
  delayMicroseconds(100);
  long acumulador = 0;
  for (uint8_t i = 0; i < 4; i++) {
    acumulador += analogRead(pinAnalogico);
    delayMicroseconds(50);
  }
  long promedio = acumulador / 4;
  long mapeado = (promedio * 100L) / 1023L;
  return (uint8_t)constrain(mapeado, 0, 100);
}

void muestrearPotenciometros() {
  coordX = procesarCanalADC(PIN_POT_X);
  coordY = procesarCanalADC(PIN_POT_Y);
}

void transmitirCoordenadas() {
  enlaceMaestro.write(coordX);
  enlaceMaestro.write((uint8_t)(coordY + 128));
}

void recibirRespuestaMaestro() {
  while (enlaceMaestro.available() > 0) {
    uint8_t dato = enlaceMaestro.read();
    ultimoMensajeRecibido = millis();

    if (dato == 0x00) {
      estadoJuego = 'I';
      nivelX = 0;
      nivelY = 0;
    } else if (dato == 0x01) {
      estadoJuego = 'V';
    } else if (dato == 0x02) {
      estadoJuego = 'D';
    } else if ((dato & 0xF0) == 0x10) {
      estadoJuego = 'J';
      nivelX = (dato >> 2) & 0x03;
      nivelY = dato & 0x03;
    }
  }
}

uint8_t mapearPWM(uint8_t nivel) {
  switch (nivel) {
    case 0: return PWM_FRIO;
    case 1: return PWM_TIBIO;
    case 2: return PWM_CALIENTE;
    case 3: return PWM_AQUI;
    default: return 0;
  }
}

void actualizarLeds() {
  if (estadoJuego == 'I' || estadoJuego == 'D') {
    analogWrite(PIN_LED_X, 0);
    analogWrite(PIN_LED_Y, 0);
  } else if (estadoJuego == 'J') {
    analogWrite(PIN_LED_X, mapearPWM(nivelX));
    analogWrite(PIN_LED_Y, mapearPWM(nivelY));
  } else if (estadoJuego == 'V') {
    bool encendido = ((millis() / 250) % 2) == 0;
    analogWrite(PIN_LED_X, encendido ? 255 : 0);
    analogWrite(PIN_LED_Y, encendido ? 255 : 0);
  }
}

void animarPerdidaEnlace() {
  bool alternar = ((millis() / 500) % 2) == 0;
  analogWrite(PIN_LED_X, alternar ? 255 : 0);
  analogWrite(PIN_LED_Y, alternar ? 0 : 255);
}

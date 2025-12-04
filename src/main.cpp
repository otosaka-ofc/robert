#include <Arduino.h>
#include <Servo.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define DHTTYPE DHT11

Servo miServo;

// ! Pines
const int servoPin = 10;
const int ldrDerechaPin = A0;
const int ldrIzquierdaPin = A1;
const int relayAvanzar = 2;
const int relayRetroceder = 3;
const int relayFreno = 4;
const int triggerAdelante = 5;
const int echoAdelante = 6;
const int triggerAtras = 9;
const int echoAtras = 8;
const int buzzerPin = 11;
const int dhtPin = 13;
const int sensorSonidoPin = A2;
const int sensorAguaPin = A3; // Se usará como sensor de lluvia por similitud

// * Configuración
int diferenciaLDR = 158;
float delayMS;

// * Timón
const int gradosIzquierda = 0;
const int gradosDerecha = 37;
const int gradosCentro = 18; // debe ser entero, no float

const int intensidadEspacio = 300;

// * Variables globales
String estadoTimon = "CENTRO";
int valorLDRDerecha;
int valorLDRIzquierda;
String direccionLuz = "NINGUNA";
bool seguirLuz = true;
int intensidadActual;
String ultimaAccion = "FRENAR";
long distanciaAdelante;
long distanciaAtras;
bool alarmas = false;
int nivelSonido;
int nivelAgua;

DHT dht(dhtPin, DHTTYPE);

unsigned long ultimoDHT = 0;
const unsigned long intervaloDHT = 2000; // leer cada 2s

unsigned long ultimoPulso = 0;
const unsigned long intervaloPulso = 300; // tiempo mínimo entre pulsos

// * Prototipos
void girarDerecha();
void girarIzquierda();
void girarCentro();

void avanzar();
void retroceder();
void frenar();

long medirDistanciaAdelante();
long medirDistanciaAtras();

float leerTemperatura();
float leerHumedad();
void mostrarClima();

// ! Envío de datos por Serial en formato JSON
void enviarTemperaturaHumedad();
void enviarDistancias();
void enviarDireccionLuz();
void enviarIntensidadLuz();
void enviarEstadoTimon();
void enviarNivelSonido();
void enviarUltimaAccion();
void enviarNivelLluvia();
void enviarEstadoAlarma();
void enviarSeguirLuz();

void setup()
{
    Serial.begin(9600);

    pinMode(relayAvanzar, OUTPUT);
    pinMode(relayRetroceder, OUTPUT);
    pinMode(relayFreno, OUTPUT);

    pinMode(triggerAdelante, OUTPUT);
    pinMode(echoAdelante, INPUT);
    pinMode(triggerAtras, OUTPUT);
    pinMode(echoAtras, INPUT);

    pinMode(buzzerPin, OUTPUT);

    pinMode(sensorSonidoPin, INPUT);

    pinMode(sensorAguaPin, INPUT);

    dht.begin();

    miServo.attach(servoPin);
    girarCentro();
}

void loop()
{
    // ==== RECEPCIÓN DE COMANDOS POR SERIAL ====
    if (Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "alarmas_on")
        {
            alarmas = true;
        }
        else if (cmd == "alarmas_off")
        {
            alarmas = false;
        }
        else if (cmd == "seguir_luz")
        {
            seguirLuz = true;
        }
        else if (cmd == "no_seguir")
            seguirLuz = false;
    }
    valorLDRDerecha = analogRead(ldrDerechaPin);
    valorLDRIzquierda = analogRead(ldrIzquierdaPin);

    nivelSonido = analogRead(sensorSonidoPin);
    enviarNivelSonido();

    nivelAgua = analogRead(sensorAguaPin);
    enviarNivelLluvia();

    if (millis() - ultimoDHT >= intervaloDHT)
    {
        ultimoDHT = millis();
        enviarTemperaturaHumedad();
    }

    // Compensación
    int ldrDerechaCorregido = valorLDRDerecha - diferenciaLDR;
    if (ldrDerechaCorregido < 0)
        ldrDerechaCorregido = 0;
    valorLDRDerecha = ldrDerechaCorregido;

    intensidadActual = max(valorLDRDerecha, valorLDRIzquierda);

    distanciaAdelante = medirDistanciaAdelante();
    distanciaAtras = medirDistanciaAtras();
    enviarDistancias();

    if ((distanciaAdelante < 7 || distanciaAtras < 7) && alarmas)
    {
        digitalWrite(buzzerPin, HIGH);
        delay(200);
        digitalWrite(buzzerPin, LOW);
    }

    // *** Lógica del timón según luz ***
    if (valorLDRDerecha > valorLDRIzquierda + 50 && intensidadActual > 250)
    {
        direccionLuz = "DERECHA";
        girarDerecha();
        if (seguirLuz)
        {
            avanzar();
        }
        else
            frenar();
    }
    else if (valorLDRIzquierda > valorLDRDerecha + 50 && intensidadActual > 250)
    {
        direccionLuz = "IZQUIERDA";
        girarIzquierda();
        if (seguirLuz)
        {
            avanzar();
        }
        else
            frenar();
    }
    else if (abs(valorLDRDerecha - valorLDRIzquierda) <= 50 && intensidadActual > 250)
    {
        direccionLuz = "CENTRO";
        girarCentro();
        if (seguirLuz)
        {
            avanzar();
        }
        else
            frenar();
    }
    else
    {
        direccionLuz = "NINGUNA";
        frenar();
    }
    enviarDireccionLuz();
    enviarEstadoTimon();
    enviarUltimaAccion();
    enviarIntensidadLuz();
    enviarEstadoAlarma();
    enviarSeguirLuz();

    delay(80);
}

// * Timón
void girarDerecha()
{
    miServo.write(gradosDerecha);
    estadoTimon = "DERECHA";
}

void girarIzquierda()
{
    miServo.write(gradosIzquierda);
    estadoTimon = "IZQUIERDA";
}

void girarCentro()
{
    miServo.write(gradosCentro);
    estadoTimon = "CENTRO";
}

// * Motor
void frenar()
{
    if (ultimaAccion == "FRENAR")
        return;

    ultimaAccion = "FRENAR";

    digitalWrite(relayAvanzar, LOW);
    digitalWrite(relayRetroceder, LOW);

    digitalWrite(relayFreno, HIGH);
    delay(200);
    digitalWrite(relayFreno, LOW);
}

void avanzar()
{
    if (ultimaAccion == "AVANZAR")
        return;
    if (ultimaAccion == "RETROCEDER")
        return;

    ultimaAccion = "AVANZAR";

    unsigned long ahora = millis();

    // no permitir avanzar muy seguido
    if (ahora - ultimoPulso < intervaloPulso)
        return;

    ultimoPulso = ahora;

    // Desactivar retroceso SIEMPRE antes de avanzar
    digitalWrite(relayRetroceder, LOW);

    // Quitar freno
    digitalWrite(relayFreno, LOW);
    delay(20); // <- IMPORTANTE (necesario para que el relé cambie)

    // Activar avance
    digitalWrite(relayAvanzar, HIGH);
    delay(120); // duracion del pulso de avance
    digitalWrite(relayAvanzar, LOW);

    // Frenada suave
    ultimaAccion = "FRENAR";
    digitalWrite(relayFreno, HIGH);
    delay(50);
    digitalWrite(relayFreno, LOW);
}

void retroceder()
{
    if (ultimaAccion == "RETROCEDER")
        return; // ya retrocediendo
    if (ultimaAccion == "AVANZAR")
        return; // evita conflicto

    ultimaAccion = "RETROCEDER";

    digitalWrite(relayAvanzar, LOW);
    digitalWrite(relayFreno, LOW);
    digitalWrite(relayRetroceder, HIGH);
}

// * Ultrasonido
long medirDistanciaAdelante()
{
    digitalWrite(triggerAdelante, LOW);
    delayMicroseconds(2);
    digitalWrite(triggerAdelante, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerAdelante, LOW);

    long duracion = pulseIn(echoAdelante, HIGH);
    return (duracion * 0.034) / 2;
}

long medirDistanciaAtras()
{
    digitalWrite(triggerAtras, LOW);
    delayMicroseconds(2);
    digitalWrite(triggerAtras, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerAtras, LOW);

    long duracion = pulseIn(echoAtras, HIGH);
    return (duracion * 0.034) / 2;
}

// * DHT11
float leerTemperatura()
{
    float t = dht.readTemperature();
    if (isnan(t))
        return NAN;
    return t;
}

float leerHumedad()
{
    float h = dht.readHumidity();
    if (isnan(h) || h < 0 || h > 100)
        return NAN; // evita 160%
    return h;
}

void mostrarClima()
{
    float temp = leerTemperatura();
    float hum = leerHumedad();

    Serial.println(F("---- DHT11 ----"));

    Serial.print(F("Temperatura: "));
    if (!isnan(temp))
        Serial.print(temp);
    else
        Serial.print(F("ERROR"));
    Serial.println(F(" °C"));

    Serial.print(F("Humedad: "));
    if (!isnan(hum))
        Serial.print(hum);
    else
        Serial.print(F("ERROR"));
    Serial.println(F(" %"));

    Serial.println();
}

// ! Envío de datos por Serial en formato JSON
void enviarTemperaturaHumedad()
{
    StaticJsonDocument<128> doc;
    if (isnan(leerTemperatura()) || isnan(leerHumedad()))
    {

        return;
    }
    doc["temperature"] = leerTemperatura();
    doc["humedity"] = leerHumedad();
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarDistancias()
{
    StaticJsonDocument<128> doc;
    doc["distance_front"] = distanciaAdelante;
    doc["distance_back"] = distanciaAtras;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarDireccionLuz()
{
    StaticJsonDocument<128> doc;
    doc["light_direction"] = direccionLuz;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarIntensidadLuz()
{
    StaticJsonDocument<128> doc;
    doc["light_intensity"] = intensidadActual;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarEstadoTimon()
{
    StaticJsonDocument<128> doc;
    doc["steering_status"] = estadoTimon;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarUltimaAccion()
{
    StaticJsonDocument<128> doc;
    doc["last_action"] = ultimaAccion;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarNivelSonido()
{
    StaticJsonDocument<128> doc;
    doc["sound_level"] = nivelSonido;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarNivelLluvia()
{
    StaticJsonDocument<128> doc;
    doc["rain_level"] = nivelAgua;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarEstadoAlarma()
{
    StaticJsonDocument<128> doc;
    doc["alarm_status"] = alarmas;
    serializeJson(doc, Serial);
    Serial.println();
}

void enviarSeguirLuz()
{
    StaticJsonDocument<128> doc;
    doc["follow_light"] = seguirLuz;
    serializeJson(doc, Serial);
    Serial.println();
}
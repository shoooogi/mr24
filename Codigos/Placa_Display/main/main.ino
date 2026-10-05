// Placa Display - Main (LoopBackMode Test - Single Core CAN)
#include <Arduino.h>
#include "include/comunicacao2515.h"
#include "include/can_random_sender.h"
#include "include/encoder.c"
#include "include/display.c"
#include "include/leds.c"
#include "include/combustivel.c"

bool initialized = false, setup0Completed = false;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(2, OUTPUT);
    Serial.begin(115200);
    Serial.println(F("[DISPLAY] Iniciando..."));
    randomSeed(analogRead(A0) + millis());
    setupDisplay();
    setupEncoder();
    setupComb();
    setup0Completed = true;
}

void setup1() {
    while (!setup0Completed) delay(1);
    Serial.println(F("[DISPLAY] Core1: Inicializando CAN..."));
    if (setupComunicacao()) Serial.println(F("[DISPLAY] CAN OK"));
    else Serial.println(F("[DISPLAY] CAN FALHOU"));
    setupRandomCAN();  // enableRandomCAN = false
    initialized = true;
    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    while (!initialized) delay(100);
    
    // TESTE LOOPBACK: envia 1 frame, recebe, pisca LED
    static uint32_t lastTest = 0;
    static int testId = 1;
    uint32_t now = millis();
    
    if (now - lastTest >= 1000) {  // 1 frame/seg
        lastTest = now;
        
        uint8_t buf[8] = {0};
        pack_float(12.34f, buf);
        if (sendCanFrame(testId, CAN_DLC[testId], buf)) {
            Serial.print(F("[LOOPBACK] TX id=")); Serial.print(testId); Serial.println(F(" OK"));
        } else {
            Serial.print(F("[LOOPBACK] TX id=")); Serial.print(testId); Serial.println(F(" FAIL"));
        }
        testId = (testId % 11) + 1;
    }
    
    // Processa RX (polling)
    receiveCan();
    
    // Watchdog
    can_conn = isCanAlive();
    
    // Atualiza display
    float vel, rpm, tensao, tempCvt, tempAmb, pedal, pressaoFreio, lat, lon;
    int32_t nivelFreio, nivelComb;
    bool sdRw, gpsFix;
    getDisplayData(vel, rpm, tensao, tempCvt, tempAmb, nivelFreio, nivelComb,
                   pedal, pressaoFreio, lat, lon, sdRw, gpsFix);
    
    // Variáveis globais para display.c
    ::vel = (int)vel; ::rpm = (int)rpm; ::tensao = tensao;
    ::TCvt = tempCvt; ::TProtecao = tempAmb; ::nivelFreio = nivelFreio;
    ::comb = (short)nivelComb; ::posAcelerador = pedal; ::pressFreio = pressaoFreio;
    ::latitudeCan = lat; ::longitudeCan = lon; ::sd_rw = sdRw; ::gps_conn = gpsFix;
    
    updateHUDMain(true, false, false);
    delay(10);
}

void loop1() {
    // Core 1 livre - sem CAN
    updateEncoder();
    delay(5);
}
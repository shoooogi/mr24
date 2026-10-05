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

    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 2000) {
       lastDebug = millis();
       const __FlashStringHelper* modeStr = (CAN_MODE_NORMAL == ACAN2515Settings::NormalMode) ? F("NormalMode") : F("LoopBackMode");
       Serial.print(F("[DBG] mode=")); Serial.print(modeStr);
       Serial.print(F(" canConnected=")); Serial.print(canConnected);
       Serial.print(F(" can_conn=")); Serial.print(can_conn);
       Serial.print(F(" rpm=")); Serial.print(rpm);
       Serial.print(F(" vel=")); Serial.println(vel);
    }
    
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

    // Formata strings para o display (trpm, tvel)
    char trpm_local[10];
    char tvel_local[10];
    sprintf(trpm_local, "%d", ::rpm);
    sprintf(tvel_local, "%d", ::vel);
    strcpy((char*)trpm, trpm_local);
    strcpy((char*)tvel, tvel_local);

    updateHUDMain(true, false, false);
    delay(10);
}

void loop1() {
    // Core 1 livre - sem CAN
    updateEncoder();
    delay(5);
}

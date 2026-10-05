/**
 * Gerador de dados CAN aleatórios para teste da Placa Display (MODO TESTE APENAS).
 *
 * Simula a Placa Central enviando quadros CAN com o mesmo protocolo
 * (mesmos IDs e mesma ordem de bytes Little-endian), para que o código
 * de recepção da Placa Display possa ser testado sem hardware real.
 *
 * ATENÇÃO: Desativado por padrão (enableRandomCAN = false).
 * Só habilite para teste de bancada SEM a Placa Central conectada.
 */

#ifndef _CAN_RANDOM_SENDER_H
#define _CAN_RANDOM_SENDER_H

#include "can_protocol.h"
#include "comunicacao2515.h"  // usa o objeto 'can' e sendCanFrame()
#include <Arduino.h>

// Controle do gerador: ligue/desligue com esta variável.
// PADRÃO = false (gerador DESATIVADO em produção).
bool enableRandomCAN = true;  // ATIVADO para teste de loopback

// Intervalo entre quadros (ms)
static unsigned long ultimoEnvio = 0;
static const unsigned long INTERVALO_MS = 200;  // mais lento para não saturar

// ID rotativo 1..11
static int idAtual = 1;

// Embaralhamento de bytes Little-endian (usa helpers do can_protocol.h)
static void packFloat(float f, uint8_t* buf)   { memcpy(buf, &f, 4); }
static void packInt32(int32_t i, uint8_t* buf) { memcpy(buf, &i, 4); }

void setupRandomCAN() {
    // Semente baseada em ruído analógico e tempo de boot
    unsigned long seed = analogRead(A0) + millis();
    if (seed == 0) seed = 0xDEADBEEF;
    randomSeed(seed);

    ultimoEnvio = millis();
    idAtual = 1;
    Serial.println(F("[RANDOM_CAN] Inicializado (DESATIVADO por padrão)"));
}

/**
 * Envia UM quadro CAN por vez (rotativo, id 1..11) simulando a Placa Central.
 * Cada quadro segue o protocolo can_protocol.h (float/int32 little-endian).
 */
void sendRandomCAN() {
    if (!enableRandomCAN) return;

    unsigned long agora = millis();
    if (agora - ultimoEnvio < INTERVALO_MS) return;
    ultimoEnvio = agora;

    // Processa quadros recebidos antes de enviar (limpa RX)
    receiveCan();

    // Valores aleatórios plausíveis
    float vel         = random(0, 121);           // 0..120 km/h
    float rpm         = random(0, 6001);          // 0..6000 rpm
    float tensaoBat   = (1200 + random(250)) / 100.0f;  // 12.00..14.50 V
    float tmpCvt      = (600 + random(61)) / 10.0f;      // 60.0..121.0 °C
    float tmpAmb      = (150 + random(26)) / 10.0f;      // 15.0..40.0 °C
    int32_t nivelFreio = random(0, 4);            // 0..3
    int32_t nivelComb  = random(0, 3);            // 0..2
    float pressaoFreio = random(0, 2001) / 100.0f;      // 0.00..20.00 MPa
    float pedal       = random(0, 101) / 100.0f;         // 0.0..1.0
    float latitude    = (-2300 + random(2001)) / 100.0f; // -23.00..-21.00
    float longitude   = (-4700 + random(2001)) / 100.0f; // -47.00..-45.00
    uint8_t sdrw      = random(0, 2);
    uint8_t fix_gps   = random(0, 2);

    idAtual = (idAtual % 11) + 1;

    uint8_t buf[8];
    bool ok = false;

    switch (idAtual) {
        case CAN_ID_VEL:           // 1
            packFloat(vel, buf);
            ok = sendCanFrame(CAN_ID_VEL, CAN_DLC[CAN_ID_VEL], buf);
            break;
        case CAN_ID_TENSAO_BAT:    // 2
            packFloat(tensaoBat, buf);
            ok = sendCanFrame(CAN_ID_TENSAO_BAT, CAN_DLC[CAN_ID_TENSAO_BAT], buf);
            break;
        case CAN_ID_TEMP_CVT_AMB:  // 3
            packFloat(tmpCvt, buf);
            packFloat(tmpAmb, buf + 4);
            ok = sendCanFrame(CAN_ID_TEMP_CVT_AMB, CAN_DLC[CAN_ID_TEMP_CVT_AMB], buf);
            break;
        case CAN_ID_RPM:           // 4
            packFloat(rpm, buf);
            ok = sendCanFrame(CAN_ID_RPM, CAN_DLC[CAN_ID_RPM], buf);
            break;
        case CAN_ID_NIVEL_FREIO:   // 5
            packInt32(nivelFreio, buf);
            ok = sendCanFrame(CAN_ID_NIVEL_FREIO, CAN_DLC[CAN_ID_NIVEL_FREIO], buf);
            break;
        case CAN_ID_LATITUDE:      // 6
            packFloat(latitude, buf);
            ok = sendCanFrame(CAN_ID_LATITUDE, CAN_DLC[CAN_ID_LATITUDE], buf);
            break;
        case CAN_ID_LONGITUDE:     // 7
            packFloat(longitude, buf);
            ok = sendCanFrame(CAN_ID_LONGITUDE, CAN_DLC[CAN_ID_LONGITUDE], buf);
            break;
        case CAN_ID_FLAGS:         // 8
            buf[0] = sdrw;
            buf[1] = fix_gps;
            ok = sendCanFrame(CAN_ID_FLAGS, CAN_DLC[CAN_ID_FLAGS], buf);
            break;
        case CAN_ID_NIVEL_COMB:    // 9
            packInt32(nivelComb, buf);
            ok = sendCanFrame(CAN_ID_NIVEL_COMB, CAN_DLC[CAN_ID_NIVEL_COMB], buf);
            break;
        case CAN_ID_PEDAL:         // 10
            packFloat(pedal, buf);
            ok = sendCanFrame(CAN_ID_PEDAL, CAN_DLC[CAN_ID_PEDAL], buf);
            break;
        case CAN_ID_PRESSAO_FREIO: // 11
            packFloat(pressaoFreio, buf);
            ok = sendCanFrame(CAN_ID_PRESSAO_FREIO, CAN_DLC[CAN_ID_PRESSAO_FREIO], buf);
            break;
    }

    if (ok) {
        Serial.print(F("[TX-CAN] id=")); Serial.print(idAtual); Serial.println(F(" OK"));
    } else {
        Serial.print(F("[TX-CAN] FAIL id=")); Serial.println(idAtual);
    }
}

#endif // _CAN_RANDOM_SENDER_H
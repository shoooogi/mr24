/**
 * Placa Display - Comunicação CAN (MCP2515 via ACAN2515)
 * Versão corrigida: NormalMode, protocolo padronizado (float/int32), sem mutex (FreeRTOS não disponível).
 */

#ifndef COMUNICACAO2515_H
#define COMUNICACAO2515_H

#include "can_protocol.h"
#include <ACAN2515.h>
#include <Arduino.h>
#include <SPI.h>

// ==================== Pinos SPI (específicos da Placa Display) ====================
#ifndef MCP2515_SCK
#define MCP2515_SCK   6
#define MCP2515_MOSI  7
#define MCP2515_MISO  4
#define MCP2515_CS    5
#endif

// ==================== Variáveis de estado do Display ====================
// Atualizadas pelo receiveCan() - declaradas volatile para acesso dual-core
volatile bool canConnected = false;
volatile uint32_t lastCanRxTime = 0;

// Dados recebidos (voláteis pois lidos no core 0, escritos no core 1)
volatile float displayVel = 0.0f;
volatile float displayRpm = 0.0f;
volatile float displayTensao = 12.0f;
volatile float displayTempCvt = 0.0f;
volatile float displayTempAmb = 0.0f;
volatile int32_t displayNivelFreio = 0;
volatile int32_t displayNivelComb = 0;
volatile float displayPedal = 0.0f;
volatile float displayPressaoFreio = 0.0f;
volatile float displayLatitude = 0.0f;
volatile float displayLongitude = 0.0f;
volatile bool displaySdRw = false;
volatile bool displayGpsFix = false;

// Flags de atualização (para evitar leitura parcial no core 0)
volatile uint32_t dataVersion = 0;  // incrementa a cada atualização completa

// Variáveis globais legadas (compatibilidade com display.c)
volatile bool can_conn = false;
volatile bool update = false;
volatile bool setLap = false;
volatile int nivelFreio = 0;
volatile short comb = 0;
volatile int vel = 0, velGps = 0, rpm = 0, rpmMovida = 0, volta = 0;
volatile float tensao = 12, pressFreio = 0, posAcelerador = 0;
volatile long told = 0, tlap = 0, tlapOld = 0, tOn = 0;
volatile double TCvt = 0, TProtecao = 0;
volatile double latitudeCan = 0.0, longitudeCan = 0.0;
volatile bool gps_conn = false, sd_rw = false, CVT = false;

// Strings para display
volatile char tvel[10];
volatile char trpm[10];
volatile char mrpm[10];

// Objeto CAN global
ACAN2515 can(MCP2515_CS, SPI, 255);  // INT pino não usado (polling)

// ==================== Inicialização CAN ====================
static bool setupComunicacao() {
    SPI.setSCK(MCP2515_SCK);
    SPI.setTX(MCP2515_MOSI);
    SPI.setRX(MCP2515_MISO);
    SPI.setCS(MCP2515_CS);
    SPI.begin();

    ACAN2515Settings settings(CAN_QUARTZ_FREQUENCY_HZ, CAN_BITRATE_BPS);
    settings.mRequestedMode = CAN_MODE_NORMAL;  // CORRIGIDO: era LoopBackMode
    settings.mOneShotModeEnabled = true;
    settings.mReceiveBufferSize = 32;
    settings.mTransmitBuffer0Size = 16;
    settings.mTransmitBuffer1Size = 16;
    settings.mTransmitBuffer2Size = 16;
    settings.mRolloverEnable = true;

    uint16_t errorCode = can.begin(settings, nullptr);  // SEM ISR = polling only

    if (errorCode == 0) {
        canConnected = true;
        const __FlashStringHelper* modeStr = (CAN_MODE_NORMAL == ACAN2515Settings::NormalMode) ? F("NormalMode") : F("LoopBackMode");
        Serial.print(F("[CAN] Inicialização OK ("));
        Serial.print(modeStr);
        Serial.println(F(", 125kbps, 20MHz"));
        return true;
    } else {
        canConnected = false;
        Serial.print(F("[CAN] ERRO init: 0x")); Serial.println(errorCode, HEX);
        return false;
    }
}

// ==================== Processamento de um frame recebido ====================
static void handleFrame(const CANMessage& frame) {
    lastCanRxTime = millis();

    // Verifica DLC esperado
    if (frame.id >= 1 && frame.id <= 11) {
        uint8_t expectedDlc = CAN_DLC[frame.id];
        if (frame.len != expectedDlc) {
            Serial.print(F("[CAN] DLC mismatch id=")); Serial.print(frame.id);
            Serial.print(F(" esperado=")); Serial.print(expectedDlc);
            Serial.print(F(" recebido=")); Serial.println(frame.len);
        }
    }

    switch (frame.id) {
        case CAN_ID_VEL:           // 1: float vel
            displayVel = unpack_float(frame.data);
            vel = (int)displayVel;
            break;

        case CAN_ID_TENSAO_BAT:    // 2: float tensao
            displayTensao = unpack_float(frame.data);
            tensao = displayTensao;
            break;

        case CAN_ID_TEMP_CVT_AMB:  // 3: float tempCvt + float tempAmb
            displayTempCvt = unpack_float(frame.data);
            displayTempAmb = unpack_float(frame.data + 4);
            TCvt = displayTempCvt;
            TProtecao = displayTempAmb;
            break;

        case CAN_ID_RPM:           // 4: float rpm
            displayRpm = unpack_float(frame.data);
            rpm = (int)displayRpm;
            break;

        case CAN_ID_NIVEL_FREIO:   // 5: int32 nivelFreio
            displayNivelFreio = unpack_int32(frame.data);
            nivelFreio = (int)displayNivelFreio;
            break;

        case CAN_ID_LATITUDE:      // 6: float latitude
            displayLatitude = unpack_float(frame.data);
            latitudeCan = displayLatitude;
            break;

        case CAN_ID_LONGITUDE:     // 7: float longitude
            displayLongitude = unpack_float(frame.data);
            longitudeCan = displayLongitude;
            break;

        case CAN_ID_FLAGS:         // 8: uint8 sdrw + uint8 fix_gps
            displaySdRw = unpack_uint8(frame.data);
            displayGpsFix = unpack_uint8(frame.data + 1);
            sd_rw = displaySdRw;
            gps_conn = displayGpsFix;
            break;

        case CAN_ID_NIVEL_COMB:    // 9: int32 nivelComb
            displayNivelComb = unpack_int32(frame.data);
            comb = (short)displayNivelComb;
            break;

        case CAN_ID_PEDAL:         // 10: float pedal
            displayPedal = unpack_float(frame.data);
            posAcelerador = displayPedal;
            break;

        case CAN_ID_PRESSAO_FREIO: // 11: float pressaoFreio
            displayPressaoFreio = unpack_float(frame.data);
            pressFreio = displayPressaoFreio;
            break;

        default:
            break;
    }

    dataVersion++;
    can_conn = true;
    update = true;
}

// ==================== Recepção CAN (polling sem mutex) ====================
/**
 * Processa todos os frames disponíveis no buffer RX.
 * Deve ser chamado periodicamente (ex.: no loop1 do core 1).
 * Retorna número de frames processados.
 */
static int receiveCan() {
    if (!canConnected) return 0;
    can.poll();  // Processa interrupções TX/RX

    int count = 0;
    CANMessage frame;
    while (can.available()) {
        if (can.receive(frame)) {
            handleFrame(frame);
            count++;
        }
    }
    return count;
}

// ==================== Envio CAN (para testes/ACK se necessário) ====================
/**
 * Envia um frame CAN genérico.
 * Retorna true se enfileirado com sucesso.
 */
static bool sendCanFrame(uint32_t id, uint8_t len, const uint8_t* data) {
    if (!canConnected) return false;

    CANMessage frame;
    frame.ext = false;
    frame.rtr = false;
    frame.id = id;
    frame.len = len;
    memcpy(frame.data, data, len);
    return can.tryToSend(frame);
}

// ==================== Watchdog de comunicação ====================
/**
 * Verifica se a comunicação CAN está ativa (recebeu frame nos últimos CAN_WATCHDOG_MS).
 */
static bool isCanAlive() {
    if (!canConnected) return false;
    uint32_t now = millis();
    if (lastCanRxTime == 0) return (now < 10000);  // tolerância no boot
    return (now - lastCanRxTime) < CAN_WATCHDOG_MS;
}

// ==================== Getters thread-safe para o core 0 (display) ====================
// Lê snapshot consistente dos dados (usa dataVersion para detectar tear)
static void getDisplayData(float& vel, float& rpm, float& tensao,
                           float& tempCvt, float& tempAmb, int32_t& nivelFreio,
                           int32_t& nivelComb, float& pedal, float& pressaoFreio,
                           float& lat, float& lon, bool& sdRw, bool& gpsFix) {
    uint32_t ver1, ver2;
    do {
        ver1 = dataVersion;
        vel        = displayVel;
        rpm        = displayRpm;
        tensao     = displayTensao;
        tempCvt    = displayTempCvt;
        tempAmb    = displayTempAmb;
        nivelFreio = displayNivelFreio;
        nivelComb  = displayNivelComb;
        pedal      = displayPedal;
        pressaoFreio = displayPressaoFreio;
        lat        = displayLatitude;
        lon        = displayLongitude;
        sdRw       = displaySdRw;
        gpsFix     = displayGpsFix;
        ver2 = dataVersion;
    } while (ver1 != ver2);
}

#endif // COMUNICACAO2515_H
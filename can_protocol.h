/**
 * Protocolo CAN compartilhado entre Placa Central e Placa Display
 * Incluir este arquivo em ambas as placas para garantir IDs, DLCs e layout idênticos.
 */

#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <Arduino.h>

// ============================================================
// IDs CAN (11 bits, standard frame)
// ============================================================
enum CAN_ID : uint32_t {
    CAN_ID_VEL           = 1,   // double -> será float (4 bytes)
    CAN_ID_TENSAO_BAT    = 2,   // double -> será float (4 bytes)
    CAN_ID_TEMP_CVT_AMB  = 3,   // float tmpCvt + float tmpAmb (8 bytes)
    CAN_ID_RPM           = 4,   // double -> será float (4 bytes)
    CAN_ID_NIVEL_FREIO   = 5,   // int32_t (4 bytes)
    CAN_ID_LATITUDE      = 6,   // double -> será float (4 bytes)
    CAN_ID_LONGITUDE     = 7,   // double -> será float (4 bytes)
    CAN_ID_FLAGS         = 8,   // uint8_t sdrw + uint8_t fix_gps (2 bytes)
    CAN_ID_NIVEL_COMB    = 9,   // int32_t (4 bytes)
    CAN_ID_PEDAL         = 10,  // double -> será float (4 bytes)
    CAN_ID_PRESSAO_FREIO = 11   // double -> será float (4 bytes)
};

// ============================================================
// DLC esperado para cada ID (deve ser igual nos dois lados)
// ============================================================
static const uint8_t CAN_DLC[12] = {
    0,  // índice 0 não usado
    4,  // ID 1: vel (float)
    4,  // ID 2: tensaoBat (float)
    8,  // ID 3: tempCvt (float) + tempAmb (float)
    4,  // ID 4: rpm (float)
    4,  // ID 5: nivelFreio (int32)
    4,  // ID 6: latitude (float)
    4,  // ID 7: longitude (float)
    2,  // ID 8: sdrw + fix_gps (uint8_t cada)
    4,  // ID 9: nivelComb (int32)
    4,  // ID 10: pedal (float)
    4   // ID 11: pressaoFreio (float)
};

// ============================================================
// Estruturas de dados para empacotamento/desempacotamento
// Usam float (4 bytes) em vez de double (8 bytes) para economizar banda
// ============================================================
struct __attribute__((packed)) CanFrame_Vel {
    float vel;
};

struct __attribute__((packed)) CanFrame_TensaoBat {
    float tensao;
};

struct __attribute__((packed)) CanFrame_Temp {
    float tempCvt;
    float tempAmb;
};

struct __attribute__((packed)) CanFrame_Rpm {
    float rpm;
};

struct __attribute__((packed)) CanFrame_NivelFreio {
    int32_t nivel;
};

struct __attribute__((packed)) CanFrame_LatLon {
    float valor;
};

struct __attribute__((packed)) CanFrame_Flags {
    uint8_t sdrw;
    uint8_t fix_gps;
};

struct __attribute__((packed)) CanFrame_NivelComb {
    int32_t nivel;
};

struct __attribute__((packed)) CanFrame_Pedal {
    float pedal;
};

struct __attribute__((packed)) CanFrame_PressaoFreio {
    float pressao;
};

// ============================================================
// Funções auxiliares de serialização (little-endian, nativo do RP2040)
// ============================================================
inline void pack_float(float val, uint8_t* buf) {
    memcpy(buf, &val, sizeof(float));
}

inline float unpack_float(const uint8_t* buf) {
    float val;
    memcpy(&val, buf, sizeof(float));
    return val;
}

inline void pack_int32(int32_t val, uint8_t* buf) {
    memcpy(buf, &val, sizeof(int32_t));
}

inline int32_t unpack_int32(const uint8_t* buf) {
    int32_t val;
    memcpy(&val, buf, sizeof(int32_t));
    return val;
}

inline void pack_uint8(uint8_t val, uint8_t* buf) {
    buf[0] = val;
}

inline uint8_t unpack_uint8(const uint8_t* buf) {
    return buf[0];
}

// ============================================================
// Configuração do barramento CAN (igual nas duas placas)
// ============================================================
#define CAN_QUARTZ_FREQUENCY_HZ   (20UL * 1000UL * 1000UL)  // 20 MHz
#define CAN_BITRATE_BPS           (125UL * 1000UL)           // 125 kbps
#define CAN_MODE_NORMAL           ACAN2515Settings::NormalMode

// ============================================================
// Watchdog de comunicação (tempo máximo sem receber frame crítico)
// ============================================================
#define CAN_WATCHDOG_MS           2000  // 2 segundos

#endif // CAN_PROTOCOL_H

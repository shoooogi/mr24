/**
 * Project Classes Placa Central
 * Constantes globais, configurações e estruturas de dados compartilhadas
 */

#ifndef _CONSTANTES_H
#define _CONSTANTES_H

#include <climits>  // Para INT_MAX, LONG_MAX
#include <cstdint>  // Para int32_t, uint8_t, etc.

#define DEBUG 0 // 0 Para não usar serial, 1 para usar serial

#if DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__)
#define D_print(...) Serial.print(__VA_ARGS__)
#define D_write(...) Serial.print(__VA_ARGS__)
#define D_println(...) Serial.println(__VA_ARGS__)
#else
#define D_SerialBegin(bauds)
#define D_print(...)
#define D_write(...)
#define D_println(...)
#endif

// -- Constantes Gerais --
#define INTERVALO_TIMER_MS 700
#define TEMPERATURA_CRITICA_CVT 200
#define RAIO_PNEU 22
#define QTD_SENSORES_PNEU 4
#define MINUTO_EM_MICROSSEGUNDOS 60000000UL

static const long MINUTO = 60 * 1000;

enum Nivel
{
    BAIXO,
    MEDIO,
    ALTO
};

#define SERIAL_BAUD 115200

// ============================================================
// TELEMETRIA LoRa E32 - CONFIGURÁVEL
// ============================================================
// Hardware: RP2040 UART1 (GPIO0=TX, GPIO1=RX) + GPIO2=AUX
#define TELEMETRIA_UART       Serial1   // HardwareSerial no RP2040
#define TELEMETRIA_RX         1         // GPIO1 -> TX do E32
#define TELEMETRIA_TX         0         // GPIO0 -> RX do E32
#define TELEMETRIA_AUX        2         // GPIO2 -> AUX do E32

// Configurações padrão (podem ser alteradas em runtime via EEPROM futuramente)
#define TELEMETRIA_UART_BAUD      9600    // Baud UART para comandos AT (fixo pela lib)
#define TELEMETRIA_AIR_RATE       AIR_DATA_RATE_011_48  // 1.2kbps - longo alcance
#define TELEMETRIA_POWER          POWER_17                // 17dBm - máx potência
#define TELEMETRIA_FEC            FEC_0_OFF               // FEC off - alcance máx
#define TELEMETRIA_MODE           FT_TRANSPARENT_TRANSMISSION
#define TELEMETRIA_IO_DRIVE       IO_D_MODE_PUSH_PULLS_PULL_UPS
#define TELEMETRIA_WAKEUP         WAKE_UP_250
#define TELEMETRIA_HEAD           0x7D

// Endereços e canal (CONFIGURÁVEIS - não mais hardcoded)
#define TELEMETRIA_ADDR_H         0
#define TELEMETRIA_ADDR_L         3
#define TELEMETRIA_CHAN           0x04

// Endereço para frames DadosLight (broadcast)
#define TELEMETRIA_LIGHT_ADDR_H   0
#define TELEMETRIA_LIGHT_ADDR_L   0
#define TELEMETRIA_LIGHT_CHAN     0x17

// Timeouts e retry
#define TELEMETRIA_TX_TIMEOUT_MS     2000   // Timeout envio
#define TELEMETRIA_MAX_RETRIES       3      // Máx tentativas
#define TELEMETRIA_RETRY_DELAY_MS    100    // Delay entre retries
#define TELEMETRIA_WATCHDOG_MS       30000  // 30s sem ACK -> reset módulo
#define TELEMETRIA_BOOT_DELAY_MS     5000   // Delay init módulo (não-bloqueante)

// ============================================================
// GPS
// ============================================================
#define GPS_TX 8
#define GPS_RX 9
#define GPS_BAUD 9600

// ============================================================
// SD CARD (SPI1)
// ============================================================
#define SD_RXPIN 12   // MISO
#define SD_CSPIN 13
#define SD_SCKPIN 10
#define SD_TXPIN 11   // MOSI

// Configurações SD
#define SD_FLUSH_INTERVAL_MS     10000    // Flush a cada 10s
#define SD_MAX_WRITE_BUFFER      512      // Buffer escrita (bytes)
#define SD_MIN_FREE_SPACE_KB     1024     // Espaço mínimo livre (1MB)

// ============================================================
// COMBUSTIVEL
// ============================================================
#define COMB_INFERIOR 14
#define COMB_SUPERIOR 15

// ============================================================
// CAN (SPI0 - MCP2515)
// ============================================================
#define CAN_SCKPIN 18
#define CAN_TXPIN 19
#define CAN_RXPIN 16
#define CAN_CSPIN 17
#define CAN_INPIN 3

// ============================================================
// FREIO
// ============================================================
#define NIVEL_FREIO 20
#define PRESSAO_FREIO 27

// ============================================================
// DIVISOR TENSAO
// ============================================================
#define DIV_TENSAO 26
#define BATERIA_TENSAO_MAX 13.3

// ============================================================
// RPM / VELOCIDADE / PEDAL
// ============================================================
#define RPM_INTERRUPT_PIN 21
#define VEL_INTERRUPT_PIN 22
#define PEDAL_ACELERADOR 28

// ============================================================
// I2C (MLX90614)
// ============================================================
#define I2C_SDA 4
#define I2C_SCL 5

// ============================================================
// ESTRUTURAS DE DADOS COMPARTILHADAS
// ============================================================

// Struct completa para CAN (float little-endian, 4 bytes cada)
struct DadosCAN {
    float vel;           // ID 1
    float tensaoBat;     // ID 2
    float tempCvt;       // ID 3 (bytes 0-3)
    float tempAmb;       // ID 3 (bytes 4-7)
    float rpm;           // ID 4
    int32_t nivelFreio;  // ID 5
    float latitude;      // ID 6
    float longitude;     // ID 7
    uint8_t sdrw;        // ID 8
    uint8_t fix_gps;     // ID 8
    int32_t nivelComb;   // ID 9
    float pedal;         // ID 10
    float pressaoFreio;  // ID 11
};

// Struct para telemetria fragmentada (máx 48 bytes payload por frame E32)
struct TelemetriaFrame {
    uint8_t frame_id;     // 0..N (identifica qual parte)
    uint8_t total_frames; // Total de frames no conjunto
    uint16_t crc16;       // CRC16 dos dados
    uint8_t data[48];     // Payload (máx 48 bytes para ficar seguro no E32)
};

// Struct leve para telemetria frequente (DadosLight expandida)
struct DadosTelemetria {
    float rpm;
    float vel;
    float tensaoBat;
    float tempCvt;
    float tempAmb;
    int32_t nivelComb;
    int32_t nivelFreio;
    float pedal;
    float pressaoFreio;
    float latitude;
    float longitude;
    uint8_t sdrw;
    uint8_t fix_gps;
    uint8_t errorCan;
    uint32_t timestamp;   // ms desde boot
};

// Struct completa (mantida para compatibilidade CAN)
struct DadosCompartilhamento {
    short nivelComb;
    int nivelFreio;
    float pressaoFreio;
    float pedal;
    float tensaoBat;
    float tmpCvt;
    float tmpAmb;
    float rpm;
    float vel;
    float latitude;
    float longitude;
    bool errorCan;
    bool fix_gps;
    bool sdrw;
};

// Struct leve original (mantida para compatibilidade)
struct DadosLight {
    double rpm;
    double vel;
    double tensaoBat;
};

typedef struct DadosCAN DadosCAN;
typedef struct TelemetriaFrame TelemetriaFrame;
typedef struct DadosTelemetria DadosTelemetria;
typedef struct DadosCompartilhamento DadosCompartilhamento;
typedef struct DadosLight DadosLight;

#endif // _CONSTANTES_H
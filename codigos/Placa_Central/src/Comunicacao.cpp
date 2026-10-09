// Definitions for global objects in Comunicacao.h
#include "../include/Comunicacao.h"

// CAN (MCP2515 via SPI0)
ACAN2515 can(CAN_CSPIN, SPI, CAN_INPIN);

// Telemetria LoRa E32 via HardwareSerial (UART1)
LoRa_E32 e32ttl100(&TELEMETRIA_UART, TELEMETRIA_AUX, UART_BPS_RATE_9600);

// Estado do CAN
uint16_t canErrorCode = 0;
bool canInitialized = false;
volatile uint32_t lastCanTxTime = 0;
volatile uint32_t lastCanRxTime = 0;

// Estado da Telemetria
bool telemetriaInitialized = false;
bool telemetriaModuleResponding = false;
volatile uint32_t lastTelemetriaTxTime = 0;
volatile uint32_t lastTelemetriaRxTime = 0;
uint8_t telemetriaConsecutiveFailures = 0;
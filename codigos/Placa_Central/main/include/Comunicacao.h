/**
 * Project Classes Placa Central - Comunicação CAN + LoRa
 * Versão corrigida: protocolo CAN padronizado, tratamento de erros, sem mutex (CAN single-core).
 */

#ifndef _COMUNICACAO_H
#define _COMUNICACAO_H

#include "Setupable.h"
#include "Constantes.h"
#include "can_protocol.h"
#include "../libs/acan2515-2.1.4/src/ACAN2515.h"
#include <Arduino.h>
#include <SPI.h>
#include "LoRa_E32.h"

// Objeto CAN global (definido aqui para ser acessível pelo ISR)
ACAN2515 can(CAN_CSPIN, SPI, CAN_INPIN);

// Telemetria LoRa
SoftwareSerial serialTelemetria(TELEMETRIA_RX, TELEMETRIA_TX);
LoRa_E32 e32ttl100(&serialTelemetria, TELEMETRIA_AUX, UART_BPS_RATE_9600);

// Estado do CAN
static uint16_t canErrorCode = 0;
static bool canInitialized = false;

// Timestamp da última transmissão/recepção bem-sucedida (para watchdog)
static volatile uint32_t lastCanTxTime = 0;
static volatile uint32_t lastCanRxTime = 0;

class Comunicacao
{
public:
    static Comunicacao *instance;
    static Comunicacao *Setup();
    static Comunicacao *GetInstance();

    bool Loop() { return false; }
    bool Debug() { return false; }

    // ==================== TELEMETRIA LoRa ====================
    void enviarDadosTelemetria(const DadosCompartilhamento& data) {
        ResponseStatus rs = e32ttl100.sendFixedMessage(0, 3, 0x04, &data, sizeof(DadosCompartilhamento));
        (void)rs;
    }

    void enviarDadosTelemetria(double data) {
        ResponseStatus rs = e32ttl100.sendFixedMessage(0, 3, 0x04, &data, sizeof(double));
        (void)rs;
    }

    void enviarDadosTelemetria(const DadosLight& data) {
        ResponseStatus rs = e32ttl100.sendFixedMessage(0, 0, 0x17, &data, sizeof(DadosLight));
        Serial.println(rs.getResponseDescription());
    }

    // ==================== STATUS CAN ====================
    bool getErrorCan() const { return canErrorCode == 0 && canInitialized; }
    bool isCanInitialized() const { return canInitialized; }
    uint16_t getCanErrorCode() const { return canErrorCode; }
    uint32_t getLastTxTime() const { return lastCanTxTime; }
    uint32_t getLastRxTime() const { return lastCanRxTime; }

    // ==================== ENVIO CAN (protocolo padronizado float/int32) ====================
    /**
     * Envia todos os frames CAN conforme protocolo can_protocol.h
     * Usa float (4 bytes) para valores numéricos, int32 para contadores.
     * Retorna true se todos os frames foram enfileirados com sucesso.
     * NOTA: Deve ser chamado apenas do core 0 (loop principal) para evitar concorrência SPI.
     */
    bool sendCanDataTo(const DadosCompartilhamento& data) {
        if (!canInitialized) return false;

        bool allOk = true;
        CANMessage frame;
        frame.ext = false;
        frame.rtr = false;
        uint8_t buf[8];

        auto trySend = [&](uint32_t id, uint8_t len, const uint8_t* data) -> bool {
            frame.id = id;
            frame.len = len;
            memcpy(frame.data, data, len);
            bool ok = can.tryToSend(frame);
            if (!ok) {
                Serial.print(F("[CAN] TX falhou id=")); Serial.println(id);
            }
            return ok;
        };

        // ID 1: Velocidade (float)
        pack_float(static_cast<float>(data.vel), buf);
        allOk &= trySend(CAN_ID_VEL, CAN_DLC[CAN_ID_VEL], buf);

        // ID 2: Tensão bateria (float)
        pack_float(static_cast<float>(data.tensaoBat), buf);
        allOk &= trySend(CAN_ID_TENSAO_BAT, CAN_DLC[CAN_ID_TENSAO_BAT], buf);

        // ID 3: Temp CVT + Amb (2 floats)
        pack_float(data.tmpCvt, buf);
        pack_float(data.tmpAmb, buf + 4);
        allOk &= trySend(CAN_ID_TEMP_CVT_AMB, CAN_DLC[CAN_ID_TEMP_CVT_AMB], buf);

        // ID 4: RPM (float)
        pack_float(static_cast<float>(data.rpm), buf);
        allOk &= trySend(CAN_ID_RPM, CAN_DLC[CAN_ID_RPM], buf);

        // ID 5: Nível freio (int32)
        pack_int32(data.nivelFreio, buf);
        allOk &= trySend(CAN_ID_NIVEL_FREIO, CAN_DLC[CAN_ID_NIVEL_FREIO], buf);

        // ID 6: Latitude (float)
        pack_float(static_cast<float>(data.latitude), buf);
        allOk &= trySend(CAN_ID_LATITUDE, CAN_DLC[CAN_ID_LATITUDE], buf);

        // ID 7: Longitude (float)
        pack_float(static_cast<float>(data.longitude), buf);
        allOk &= trySend(CAN_ID_LONGITUDE, CAN_DLC[CAN_ID_LONGITUDE], buf);

        // ID 8: Flags SD/GPS (2 bytes)
        buf[0] = data.sdrw ? 1 : 0;
        buf[1] = data.fix_gps ? 1 : 0;
        allOk &= trySend(CAN_ID_FLAGS, CAN_DLC[CAN_ID_FLAGS], buf);

        // ID 9: Nível combustível (int32) - CORRIGIDO: agora envia (era comentado)
        pack_int32(data.nivelComb, buf);
        allOk &= trySend(CAN_ID_NIVEL_COMB, CAN_DLC[CAN_ID_NIVEL_COMB], buf);

        // ID 10: Pedal acelerador (float) - CORRIGIDO: agora envia (era comentado)
        pack_float(static_cast<float>(data.pedal), buf);
        allOk &= trySend(CAN_ID_PEDAL, CAN_DLC[CAN_ID_PEDAL], buf);

        // ID 11: Pressão freio (float)
        pack_float(static_cast<float>(data.pressaoFreio), buf);
        allOk &= trySend(CAN_ID_PRESSAO_FREIO, CAN_DLC[CAN_ID_PRESSAO_FREIO], buf);

        if (allOk) {
            lastCanTxTime = millis();
        }

        return allOk;
    }

    // ==================== RECEPÇÃO CAN (polling) ====================
    /**
     * Processa quadros recebidos. Deve ser chamado periodicamente (core 0).
     * Retorna número de frames processados.
     */
    int processCanRx() {
        if (!canInitialized) return 0;

        int count = 0;
        CANMessage frame;
        while (can.available()) {
            if (can.receive(frame)) {
                lastCanRxTime = millis();
                count++;
                // A Placa Central não espera receber frames no uso normal,
                // mas mantemos o polling para limpar buffers e detectar bus-off.
            }
        }
        return count;
    }

private:
    Comunicacao() = default;

    // ----- Helpers de empacotamento (little-endian nativo) -----
    static inline void pack_float(float val, uint8_t* buf) { memcpy(buf, &val, sizeof(float)); }
    static inline void pack_int32(int32_t val, uint8_t* buf) { memcpy(buf, &val, sizeof(int32_t)); }

    // ----- Inicialização pinos E32 -----
    static void setupE32Pins() {
        pinMode(TELEMETRIA_AUX, OUTPUT);
        digitalWrite(TELEMETRIA_AUX, HIGH);
    }

    static void wakeupE32Pins() {
        digitalWrite(TELEMETRIA_AUX, LOW);
        delay(50);
        digitalWrite(TELEMETRIA_AUX, HIGH);
        delay(100);
    }

    static bool getConfigLoRa(Configuration& outConfig) {
        ResponseStructContainer c = e32ttl100.getConfiguration();
        outConfig = *(Configuration*)c.data;
        bool ok = (c.status.code == 0 || c.status.code == 13);
        c.close();
        return ok;
    }

    static bool setConfigLoRa(const Configuration& config) {
        ResponseStatus rs = e32ttl100.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
        return rs.code == 0;
    }

    static void printLoRaConfig(Configuration cfg) {  // remover const para evitar warning
        Serial.println(F("----------------------------------------"));
        Serial.print(F("HEAD: 0x")); Serial.println(cfg.HEAD, HEX);
        Serial.print(F("AddH: ")); Serial.println(cfg.ADDH, HEX);
        Serial.print(F("AddL: ")); Serial.println(cfg.ADDL, HEX);
        Serial.print(F("Chan: ")); Serial.println(cfg.CHAN, DEC);
        Serial.print(F("UART Baud: ")); Serial.println(cfg.SPED.getUARTBaudRate());
        Serial.print(F("Air Rate: ")); Serial.println(cfg.SPED.getAirDataRate());
        Serial.print(F("Parity: ")); Serial.println(cfg.SPED.getUARTParityDescription());
        Serial.print(F("Mode: ")); Serial.println(cfg.OPTION.getFixedTransmissionDescription());
        Serial.print(F("IO Drive: ")); Serial.println(cfg.OPTION.getIODroveModeDescription());
        Serial.print(F("Wakeup: ")); Serial.println(cfg.OPTION.getWirelessWakeUPTimeDescription());
        Serial.print(F("FEC: ")); Serial.println(cfg.OPTION.getFECDescription());
        Serial.print(F("Power: ")); Serial.println(cfg.OPTION.getTransmissionPowerDescription());
        Serial.println(F("----------------------------------------"));
    }

    // ----- Setup CAN com tratamento de erro adequado -----
    static bool setupCanBus() {
        SPI.setSCK(CAN_SCKPIN);
        SPI.setRX(CAN_RXPIN);
        SPI.setTX(CAN_TXPIN);
        SPI.setCS(CAN_CSPIN);
        SPI.begin();

        ACAN2515Settings settings(CAN_QUARTZ_FREQUENCY_HZ, CAN_BITRATE_BPS);
        settings.mRequestedMode = CAN_MODE_NORMAL;
        settings.mOneShotModeEnabled = true;           // Evita retransmissões infinitas
        settings.mReceiveBufferSize = 32;              // Buffer RX do driver
        settings.mTransmitBuffer0Size = 16;            // Buffer TX0
        settings.mTransmitBuffer1Size = 16;            // Buffer TX1
        settings.mTransmitBuffer2Size = 16;            // Buffer TX2
        settings.mRolloverEnable = true;               // RXB0 -> RXB1 rollover

        canErrorCode = can.begin(settings, [] { can.isr(); });

        if (canErrorCode == 0) {
            canInitialized = true;
            Serial.println(F("[CAN] Inicialização OK (NormalMode, 125kbps, 20MHz)"));
            // Pisca LED 3x rápido = CAN OK
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH); delay(50);
                digitalWrite(LED_BUILTIN, LOW);  delay(50);
            }
            return true;
        } else {
            canInitialized = false;
            Serial.print(F("[CAN] ERRO init: 0x")); Serial.println(canErrorCode, HEX);
            // Pisca LED padrão SOS longo = falha CAN
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH); delay(300);
                digitalWrite(LED_BUILTIN, LOW);  delay(300);
            }
            return false;
        }
    }

    // ----- Setup Telemetria LoRa -----
    static bool setupTelemetria() {
        Serial.println(F("[TELEMETRIA] Iniciando..."));
        delay(300);
        e32ttl100.begin();
        delay(5000);

        Configuration cfg;
        if (!getConfigLoRa(cfg)) {
            Serial.println(F("[TELEMETRIA] Módulo não responde"));
            return false;
        }

        // Reconfigura apenas UART e parâmetros de alcance
        cfg.SPED.uartBaudRate = UART_BPS_9600;
        cfg.SPED.airDataRate = AIR_DATA_RATE_011_48;
        cfg.SPED.uartParity = MODE_00_8N1;
        cfg.OPTION.fec = FEC_0_OFF;
        cfg.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
        cfg.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
        cfg.OPTION.transmissionPower = POWER_17;
        cfg.OPTION.wirelessWakeupTime = WAKE_UP_250;
        cfg.HEAD = 0x7D;

        if (!setConfigLoRa(cfg)) {
            Serial.println(F("[TELEMETRIA] Falha ao gravar config"));
            return false;
        }

        Serial.println(F("[TELEMETRIA] Configurada OK"));
        printLoRaConfig(cfg);
        return true;
    }

public:
    // Método público para inicialização controlada (chamado uma vez no GetInstance)
    static bool initialize() {
        bool canOk = setupCanBus();
        bool loraOk = setupTelemetria();
        (void)loraOk; // LoRa é opcional; CAN é crítico
        return canOk;
    }
};

Comunicacao *Comunicacao::instance{nullptr};

Comunicacao *Comunicacao::GetInstance() {
    if (instance == nullptr) {
        instance = new Comunicacao();
        // Inicialização real (hardware) feita aqui para capturar erros
        initialize();
    }
    return instance;
}

#endif // _COMUNICACAO_H
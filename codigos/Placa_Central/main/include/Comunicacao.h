/**
 * Project Classes Placa Central - Comunicação CAN + LoRa Telemetria
 * Versão corrigida: protocolo CAN padronizado, telemetria fragmentada, watchdog, HardwareSerial.
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

// ============================================================
// OBJETOS GLOBAIS (extern declarations - defined in .cpp)
// ============================================================

// CAN (MCP2515 via SPI0)
extern ACAN2515 can;

// Telemetria LoRa E32 via HardwareSerial (UART1)
extern LoRa_E32 e32ttl100;

// Estado do CAN
extern uint16_t canErrorCode;
extern bool canInitialized;
extern volatile uint32_t lastCanTxTime;
extern volatile uint32_t lastCanRxTime;

// Estado da Telemetria
extern bool telemetriaInitialized;
extern bool telemetriaModuleResponding;
extern volatile uint32_t lastTelemetriaTxTime;
extern volatile uint32_t lastTelemetriaRxTime;
extern uint8_t telemetriaConsecutiveFailures;

// ============================================================
// CLASSE COMUNICACAO
// ============================================================

class Comunicacao
{
public:
    static Comunicacao *instance;
    static Comunicacao *Setup();
    static Comunicacao *GetInstance();

    bool Loop() { return false; }
    bool Debug() { return false; }

    // ==================== TELEMETRIA LoRa ====================

    /**
     * Envia struct completa fragmentada em múltiplos frames LoRa.
     * Retorna true se todos os fragmentos enviados com sucesso.
     */
    bool enviarDadosTelemetria(const DadosTelemetria& data) {
        if (!telemetriaInitialized || !telemetriaModuleResponding) {
            return false;
        }
        return enviarFragmentosTelemetria(data);
    }

    // Overload para compatibilidade: converte DadosCompartilhamento -> DadosTelemetria
    bool enviarDadosTelemetria(const DadosCompartilhamento& data) {
        DadosTelemetria t;
        t.rpm = data.rpm;
        t.vel = data.vel;
        t.tensaoBat = data.tensaoBat;
        t.tempCvt = data.tmpCvt;
        t.tempAmb = data.tmpAmb;
        t.nivelComb = data.nivelComb;
        t.nivelFreio = data.nivelFreio;
        t.pedal = data.pedal;
        t.pressaoFreio = data.pressaoFreio;
        t.latitude = data.latitude;
        t.longitude = data.longitude;
        t.sdrw = data.sdrw ? 1 : 0;
        t.fix_gps = data.fix_gps ? 1 : 0;
        t.errorCan = data.errorCan ? 1 : 0;
        t.timestamp = millis();
        return enviarDadosTelemetria(t);
    }

    // Overload para compatibilidade: DadosLight (apenas 3 campos)
    bool enviarDadosTelemetria(const DadosLight& data) {
        DadosTelemetria t = {0};
        t.rpm = data.rpm;
        t.vel = data.vel;
        t.tensaoBat = data.tensaoBat;
        t.timestamp = millis();
        return enviarDadosTelemetria(t);
    }

    void enviarDadosTelemetria(double data) {
        (void)data; // não usado
    }

    // ==================== STATUS ====================
    bool getErrorCan() const { return canErrorCode == 0 && canInitialized; }
    bool isCanInitialized() const { return canInitialized; }
    uint16_t getCanErrorCode() const { return canErrorCode; }
    uint32_t getLastTxTime() const { return lastCanTxTime; }
    uint32_t getLastRxTime() const { return lastCanRxTime; }

    bool isTelemetriaInitialized() const { return telemetriaInitialized; }
    bool isTelemetriaModuleOk() const { return telemetriaModuleResponding; }
    uint8_t getTelemetriaFailures() const { return telemetriaConsecutiveFailures; }
    uint32_t getLastTelemetriaTxTime() const { return lastTelemetriaTxTime; }

    // ==================== ENVIO CAN ====================
    /**
     * Envia todos os frames CAN conforme protocolo can_protocol.h
     * Retorna true se todos os frames enfileirados com sucesso.
     * NOTA: Chamar apenas do core 0 (loop principal).
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

        // ID 9: Nível combustível (int32)
        pack_int32(data.nivelComb, buf);
        allOk &= trySend(CAN_ID_NIVEL_COMB, CAN_DLC[CAN_ID_NIVEL_COMB], buf);

        // ID 10: Pedal acelerador (float)
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

    // ==================== RECEPÇÃO CAN ====================
    int processCanRx() {
        if (!canInitialized) return 0;

        int count = 0;
        CANMessage frame;
        while (can.available()) {
            if (can.receive(frame)) {
                lastCanRxTime = millis();
                count++;
            }
        }
        return count;
    }

    // ==================== TELEMETRIA: FRAGMENTAÇÃO E ENVIO ====================

private:
    // CRC16-CCITT para integridade dos frames
    static uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
                else crc <<= 1;
            }
        }
        return crc;
    }

    // Fragmenta DadosTelemetria em frames de ≤48 bytes
    bool enviarFragmentosTelemetria(const DadosTelemetria& data) {
        // Serializa struct em buffer temporário
        uint8_t serialized[sizeof(DadosTelemetria)];
        memcpy(serialized, &data, sizeof(DadosTelemetria));

        // Calcula quantos frames necessários (máx 48 bytes payload por frame)
        const size_t max_payload = 48;
        size_t total_len = sizeof(DadosTelemetria);
        uint8_t total_frames = (total_len + max_payload - 1) / max_payload;

        if (total_frames == 0) return false;
        if (total_frames > 10) return false; // segurança

        bool all_ok = true;

        for (uint8_t frame_idx = 0; frame_idx < total_frames; frame_idx++) {
            size_t offset = frame_idx * max_payload;
            size_t chunk_len = min(max_payload, total_len - offset);

            TelemetriaFrame frame;
            frame.frame_id = frame_idx;
            frame.total_frames = total_frames;
            frame.crc16 = crc16_ccitt(serialized + offset, chunk_len);
            memset(frame.data, 0, sizeof(frame.data));
            memcpy(frame.data, serialized + offset, chunk_len);

            // Envia frame com retry
            if (!enviarFrameComRetry(frame)) {
                all_ok = false;
                Serial.print(F("[TLM] Falha frame ")); Serial.print(frame_idx);
                Serial.print(F("/")); Serial.println(total_frames);
            } else {
                Serial.print(F("[TLM] Frame ")); Serial.print(frame_idx);
                Serial.print(F("/")); Serial.println(total_frames);
            }

            // Pequeno delay entre frames para não saturar UART
            delay(10);
        }

        if (all_ok) {
            lastTelemetriaTxTime = millis();
            telemetriaConsecutiveFailures = 0;
        } else {
            telemetriaConsecutiveFailures++;
            if (telemetriaConsecutiveFailures >= TELEMETRIA_MAX_RETRIES) {
                Serial.println(F("[TLM] Falhas consecutivas - resetando módulo"));
                resetTelemetriaModule();
            }
        }

        return all_ok;
    }

    // Envia um frame TelemetriaFrame com retry
    bool enviarFrameComRetry(const TelemetriaFrame& frame) {
        for (uint8_t attempt = 0; attempt < TELEMETRIA_MAX_RETRIES; attempt++) {
            ResponseStatus rs = e32ttl100.sendFixedMessage(
                TELEMETRIA_ADDR_H, TELEMETRIA_ADDR_L, TELEMETRIA_CHAN,
                &frame, sizeof(TelemetriaFrame)
            );

            if (rs.code == 0) {
                return true; // Sucesso
            }

            Serial.print(F("[TLM] Retry ")); Serial.print(attempt + 1);
            Serial.print(F("/")); Serial.print(TELEMETRIA_MAX_RETRIES);
            Serial.print(F(" code=")); Serial.println(rs.code);
            delay(TELEMETRIA_RETRY_DELAY_MS);
        }
        return false;
    }

    // Reset do módulo via pino AUX
    void resetTelemetriaModule() {
        Serial.println(F("[TLM] Reset módulo via AUX"));
        digitalWrite(TELEMETRIA_AUX, LOW);
        delay(100);
        digitalWrite(TELEMETRIA_AUX, HIGH);
        delay(500);
        telemetriaConsecutiveFailures = 0;
        telemetriaModuleResponding = checkTelemetriaModule();
    }

    // Verifica se módulo responde
    bool checkTelemetriaModule() {
        ResponseStructContainer c = e32ttl100.getConfiguration();
        bool ok = (c.status.code == 0 || c.status.code == 13);
        c.close();
        return ok;
    }

    // ==================== PINOS E32 ====================
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

    static void printLoRaConfig(Configuration cfg) {
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

    // ==================== SETUP CAN ====================
    static bool setupCanBus() {
        SPI.setSCK(CAN_SCKPIN);
        SPI.setRX(CAN_RXPIN);
        SPI.setTX(CAN_TXPIN);
        SPI.setCS(CAN_CSPIN);
        SPI.begin();

        ACAN2515Settings settings(CAN_QUARTZ_FREQUENCY_HZ, CAN_BITRATE_BPS);
        settings.mRequestedMode = CAN_MODE_NORMAL;
        settings.mOneShotModeEnabled = true;
        settings.mReceiveBufferSize = 32;
        settings.mTransmitBuffer0Size = 16;
        settings.mTransmitBuffer1Size = 16;
        settings.mTransmitBuffer2Size = 16;
        settings.mRolloverEnable = true;

        canErrorCode = can.begin(settings, [] { can.isr(); });

        if (canErrorCode == 0) {
            canInitialized = true;
            Serial.println(F("[CAN] Inicialização OK (NormalMode, 125kbps, 20MHz)"));
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH); delay(50);
                digitalWrite(LED_BUILTIN, LOW);  delay(50);
            }
            return true;
        } else {
            canInitialized = false;
            Serial.print(F("[CAN] ERRO init: 0x")); Serial.println(canErrorCode, HEX);
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_BUILTIN, HIGH); delay(300);
                digitalWrite(LED_BUILTIN, LOW);  delay(300);
            }
            return false;
        }
    }

    // ==================== SETUP TELEMETRIA ====================
    static bool setupTelemetria() {
        Serial.println(F("[TLM] Iniciando..."));
        setupE32Pins();

        // Inicializa HardwareSerial UART1 (pinos já definidos no variant do RP2040)
        TELEMETRIA_UART.begin(TELEMETRIA_UART_BAUD);
        delay(100);

        // Acorda módulo via AUX
        wakeupE32Pins();
        delay(500);

        // Tenta ler configuração
        uint32_t start = millis();
        Configuration cfg;
        bool config_ok = false;

        while (millis() - start < TELEMETRIA_BOOT_DELAY_MS) {
            if (getConfigLoRa(cfg)) {
                config_ok = true;
                break;
            }
            delay(100);
        }

        if (!config_ok) {
            Serial.println(F("[TLM] Módulo não responde após timeout"));
            telemetriaModuleResponding = false;
            telemetriaInitialized = false;
            return false;
        }

        // Reconfigura apenas parâmetros essenciais
        cfg.SPED.uartBaudRate = UART_BPS_9600;
        cfg.SPED.airDataRate = TELEMETRIA_AIR_RATE;
        cfg.SPED.uartParity = MODE_00_8N1;
        cfg.OPTION.fec = TELEMETRIA_FEC;
        cfg.OPTION.fixedTransmission = TELEMETRIA_MODE;
        cfg.OPTION.ioDriveMode = TELEMETRIA_IO_DRIVE;
        cfg.OPTION.transmissionPower = TELEMETRIA_POWER;
        cfg.OPTION.wirelessWakeupTime = TELEMETRIA_WAKEUP;
        cfg.HEAD = TELEMETRIA_HEAD;

        // Preserva endereço/canal originais do módulo
        // Se quiser forçar endereços específicos, descomente:
        // cfg.ADDH = TELEMETRIA_ADDR_H;
        // cfg.ADDL = TELEMETRIA_ADDR_L;
        // cfg.CHAN = TELEMETRIA_CHAN;

        if (!setConfigLoRa(cfg)) {
            Serial.println(F("[TLM] Falha ao gravar config"));
            telemetriaModuleResponding = false;
            telemetriaInitialized = false;
            return false;
        }

        telemetriaModuleResponding = true;
        telemetriaInitialized = true;
        Serial.println(F("[TLM] Configurada OK"));
        printLoRaConfig(cfg);
        return true;
    }

public:
    // Inicialização controlada
    static bool initialize() {
        bool canOk = setupCanBus();
        bool tlmOk = setupTelemetria();
        (void)tlmOk; // Telemetria é opcional; CAN é crítico
        return canOk;
    }

    // Watchdog telemetria - chamar periodicamente (ex: no loop principal)
    static void telemetriaWatchdog() {
        if (!telemetriaInitialized) return;

        uint32_t now = millis();

        // Verifica se módulo ainda responde (a cada 30s)
        if (now - lastTelemetriaTxTime > TELEMETRIA_WATCHDOG_MS) {
            Serial.println(F("[TLM] Watchdog: sem TX há 30s, verificando módulo"));
            if (!checkTelemetriaModuleStatic()) {
                Serial.println(F("[TLM] Módulo não responde - reset"));
                telemetriaConsecutiveFailures = TELEMETRIA_MAX_RETRIES;
            }
        }

        // Se muitas falhas consecutivas, reset
        if (telemetriaConsecutiveFailures >= TELEMETRIA_MAX_RETRIES) {
            // Será tratado na próxima tentativa de envio
        }
    }

    // Versão static para uso no watchdog
    static bool checkTelemetriaModuleStatic() {
        ResponseStructContainer c = e32ttl100.getConfiguration();
        bool ok = (c.status.code == 0 || c.status.code == 13);
        c.close();
        return ok;
    }

    // Processa recepção LoRa (downlink)
    static void processTelemetriaRx() {
        if (!telemetriaInitialized) return;
        if (e32ttl100.available() > 0) {
            lastTelemetriaRxTime = millis();
            ResponseStructContainer rsc = e32ttl100.receiveMessage(255);
            // TODO: processar comando downlink se necessário
            rsc.close();
        }
    }
};

Comunicacao *Comunicacao::instance{nullptr};

Comunicacao *Comunicacao::GetInstance() {
    if (instance == nullptr) {
        instance = new Comunicacao();
        initialize();
    }
    return instance;
}

#endif // _COMUNICACAO_H
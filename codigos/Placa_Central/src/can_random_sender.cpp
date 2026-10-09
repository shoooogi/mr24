/**
 * Gerador de dados CAN aleatórios para a Placa Central (MODO DEBUG).
 *
 * Simula sensores enviando quadros CAN com o mesmo protocolo
 * (mesmos IDs e mesma ordem de bytes Little-endian), para que o código
 * da Placa Display possa ser testado sem hardware real na Central.
 *
 * ATENÇÃO: Desativado por padrão (enableRandomCAN = false).
 * Só habilite para teste de bancada SEM sensores reais.
 */

#include "../include/can_random_sender.h"
#include "../include/can_protocol.h"
#include "../include/Comunicacao.h"
#include <Arduino.h>

// Controle do gerador: ligue/desligue com esta variável.
// PADRÃO = false (gerador DESATIVADO em produção).
bool enableRandomCAN = false;

// Intervalo entre lotes completos (ms)
static unsigned long ultimoEnvio = 0;
static const unsigned long INTERVALO_MS = 700;  // mesmo timer da Central

void setupRandomCAN() {
    // Semente baseada em ruído analógico e tempo de boot
    // Nota: A0 pode não existir no RP2040, usa pino ADC disponível
    unsigned long seed = millis();
    #if defined(ARDUINO_ARCH_RP2040)
        // No RP2040, usa o pino ADC0 (GPIO26) se disponível
        seed += analogRead(26);
    #else
        seed += analogRead(A0);
    #endif
    if (seed == 0) seed = 0xDEADBEEF;
    randomSeed(seed);

    ultimoEnvio = millis();
    Serial.println(F("[RANDOM_CAN_CENTRAL] Inicializado (DESATIVADO por padrão)"));
}

/**
 * Envia TODOS os frames CAN (IDs 1..11) simulando sensores reais.
 * Substitui o envio normal de sendCanDataTo() quando ativo.
 */
void sendRandomCAN(Comunicacao* comunicacao) {
    if (!enableRandomCAN) return;
    if (!comunicacao) return;

    unsigned long agora = millis();
    if (agora - ultimoEnvio < INTERVALO_MS) return;
    ultimoEnvio = agora;

    // Processa recepção CAN antes de enviar
    comunicacao->processCanRx();

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

    DadosCompartilhamento data = {0};
    data.vel         = vel;
    data.rpm         = rpm;
    data.tensaoBat   = tensaoBat;
    data.tmpCvt      = tmpCvt;
    data.tmpAmb      = tmpAmb;
    data.nivelFreio  = nivelFreio;
    data.nivelComb   = nivelComb;
    data.pressaoFreio = pressaoFreio;
    data.pedal       = pedal;
    data.latitude    = latitude;
    data.longitude   = longitude;
    data.sdrw        = sdrw;
    data.fix_gps     = fix_gps;
    data.errorCan    = false;  // CAN OK no modo teste

    // Envia todos os frames via mecanismo padrão (já loga falhas)
    bool ok = comunicacao->sendCanDataTo(data);
    if (ok) {
        Serial.println(F("[RANDOM_CAN_CENTRAL] Lote completo enviado OK"));
    } else {
        Serial.println(F("[RANDOM_CAN_CENTRAL] Falha no lote"));
    }
}
/*
 * Project Classes Placa Central - Main
 * Versão corrigida: CAN no core 0 (loop), sensores no core 1 (timer), watchdog, sem String.
 */

#include <Arduino.h>
#include <RPi_Pico_ISR_Timer.hpp>
#include <RPi_Pico_TimerInterrupt.h>
#include <RPi_Pico_ISR_Timer.h>
#include "include/Instancia.h"
#include "include/Comunicacao.h"
#include "include/can_protocol.h"
#include "include/can_random_sender.h"  // DEBUG: dados aleatórios

// ============================================================
// DEFINIÇÕES DE VARIÁVEIS GLOBAIS (evita multiple definition)
// ============================================================

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

// Random CAN
bool enableRandomCAN = false;
static unsigned long ultimoEnvioRandom = 0;
static const unsigned long INTERVALO_MS_RANDOM = 700;

/**
 * DECLARAÇÕES DE FUNÇÕES
 */
bool UpdateSensors(struct repeating_timer *t);
bool CheckCanWatchdog(struct repeating_timer *t);

/**
 * VARIÁVEIS GLOBAIS
 */
bool setupCompleto = false;
RPI_PICO_Timer Core1Timer1(1);
Instancia *myInstance = nullptr;

// Timing para envio CAN periódico no core 0 (não em ISR)
static uint32_t lastCanTx = 0;
static const uint32_t CAN_TX_INTERVAL_MS = 700;  // mesmo INTERVALO_TIMER_MS

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    D_SerialBegin(SERIAL_BAUD);

    // DEBUG é uma constante definida em Constantes.h
    // WaitSerial(DEBUG);  // Removido - não existe no core pico

    D_println(F("INICIALIZANDO INSTANCIA"));
    D_println(F("======================="));
    myInstance = Instancia::GetInstance();  // shortcut: lazy init, verifique CAN apos

    // DEBUG: Inicializa gerador aleatório CAN (desativado por padrão)
    setupRandomCAN();

    D_println(F("======================="));
    D_println(F("INICIALIZACAO CONCLUIDA"));

    setupCompleto = true;  // shortcut: lazy init, verifique CAN antes de core 1
    D_println(F("Setup core0 finalizado."));
}

void setup1()
{
    while (!setupCompleto) {
        delay(1);
    }

    D_println(F("Setup1 (core1) iniciando"));
    delay(10);

    // Timer no core 1 para leitura de sensores (ADC, I2C, GPS, SD)
    // NÃO faz CAN/SPI aqui - evita concorrência
    if (Core1Timer1.attachInterruptInterval(INTERVALO_TIMER_MS, UpdateSensors)) {
        D_print(F("Core1Timer1 OK. Intervalo: ")); D_print(INTERVALO_TIMER_MS); D_println(F(" ms"));
    } else {
        D_println(F("Falha no Core1Timer1!"));
    }
}

// Core 0: Loop principal - envio CAN periódico + processamento RX + watchdog
void loop()
{
    if (!myInstance) return;

    uint32_t now = millis();

    // Envio CAN periódico (não em ISR, sem mutex contention com core 1)
    if (now - lastCanTx >= CAN_TX_INTERVAL_MS) {
        lastCanTx = now;

        // DEBUG: Modo aleatório substitui sensores reais
        if (enableRandomCAN) {
            sendRandomCAN(myInstance->getComunicacao());
        } else {
            // Atualiza dados que mudam no core 0 (nenhum no momento, mas mantém estrutura)
            myInstance->SincronizarDados();

            // Envia CAN (com mutex interno)
            // shortcut: falha CAN nao entra safe-state automatico, upgrade = enviar frame emergencia
            if (!myInstance->EnviarDadosCanBus()) {
                D_println(F("[WARN] Falha no envio CAN"));
            }
        }

        // Telemetria LoRa (menos frequente, pode ser a cada 2-3 ciclos CAN)
        static uint8_t loraCounter = 0;
        if (++loraCounter >= 3) {
            loraCounter = 0;
            myInstance->EnviarDadosTelemetria();
        }

        // Debug periódico
        myInstance->PrintarDados();
    }

    // Processa recepção CAN (polling)
    myInstance->ProcessarCanRx();

    // Watchdog CAN
    static bool canLost = false;
    if (!myInstance->CanWatchdogOk()) {
        if (!canLost) {
            D_println(F("[CAN] WATCHDOG: Comunicação perdida!"));
            canLost = true;
        }
        digitalWrite(LED_BUILTIN, LOW);  // falha persistente (não blink)
    } else {
        if (canLost) {
            D_println(F("[CAN] Comunicação restaurada"));
            canLost = false;
        }
    }

    // Watchdog Telemetria (chamado periodicamente)
    Comunicacao::telemetriaWatchdog();
    Comunicacao::processTelemetriaRx();

    // Escrita no SD (não bloqueante)
    myInstance->EscreverSD();

    // Flush periódico do SD (a cada 10s)
    static uint32_t lastSdFlush = 0;
    if (now - lastSdFlush >= 10000) {
        lastSdFlush = now;
        // O flush é feito automaticamente no escreverSD baseado no tempo
    }

    // Pequeno yield para não travar watchdog do sistema
    yield();
}

void loop1()
{
    // Core 1 não precisa de loop ativo - tudo no timer ISR
    // Mantido vazio por compatibilidade com framework dual-core
}

// ISR do Timer no Core 1: Leitura de sensores (ADC, I2C, GPIO)
// NÃO usa SPI/CAN/SD aqui - apenas leituras rápidas
bool UpdateSensors(struct repeating_timer *t)
{
    (void)t; // unused
    if (!myInstance) return true;

    // Pisca LED para indicar atividade do timer
    digitalWrite(LED_BUILTIN, LOW);

    // shortcut: atualiza sensores rápidos (não CAN/SD/SPI), upgrade = medir micros() se >10ms
    myInstance->SetDadosSistemas();  // Combustível, Freio, Pedal, Tensão, Temp

    digitalWrite(LED_BUILTIN, HIGH);
    return true;  // mantém timer rodando
}

// Timer adicional para watchdog CAN (opcional, pode usar o loop)
bool CheckCanWatchdog(struct repeating_timer *t)
{
    (void)t;
    // Verificação já feita no loop() do core 0
    return true;
}

// ============================================================
// RANDOM CAN DEBUG (implementação inline no .ino para evitar multiple definition)
// ============================================================

void setupRandomCAN() {
    unsigned long seed = millis();
    #if defined(ARDUINO_ARCH_RP2040)
        seed += analogRead(26);
    #else
        seed += analogRead(A0);
    #endif
    if (seed == 0) seed = 0xDEADBEEF;
    randomSeed(seed);

    ultimoEnvioRandom = millis();
    Serial.println(F("[RANDOM_CAN_CENTRAL] Inicializado (DESATIVADO por padrão)"));
}

void sendRandomCAN(Comunicacao* comunicacao) {
    if (!enableRandomCAN) return;
    if (!comunicacao) return;

    unsigned long agora = millis();
    if (agora - ultimoEnvioRandom < INTERVALO_MS_RANDOM) return;
    ultimoEnvioRandom = agora;

    comunicacao->processCanRx();

    float vel         = random(0, 121);
    float rpm         = random(0, 6001);
    float tensaoBat   = (1200 + random(250)) / 100.0f;
    float tmpCvt      = (600 + random(61)) / 10.0f;
    float tmpAmb      = (150 + random(26)) / 10.0f;
    int32_t nivelFreio = random(0, 4);
    int32_t nivelComb  = random(0, 3);
    float pressaoFreio = random(0, 2001) / 100.0f;
    float pedal       = random(0, 101) / 100.0f;
    float latitude    = (-2300 + random(2001)) / 100.0f;
    float longitude   = (-4700 + random(2001)) / 100.0f;
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
    data.errorCan    = false;

    bool ok = comunicacao->sendCanDataTo(data);
    if (ok) {
        Serial.println(F("[RANDOM_CAN_CENTRAL] Lote completo enviado OK"));
    } else {
        Serial.println(F("[RANDOM_CAN_CENTRAL] Falha no lote"));
    }
}
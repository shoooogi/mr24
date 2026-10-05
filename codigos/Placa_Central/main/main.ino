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
    myInstance = Instancia::GetInstance();

    D_println(F("======================="));
    D_println(F("INICIALIZACAO CONCLUIDA"));

    setupCompleto = true;
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
        D_println(F("Core1Timer1 OK. Intervalo: ") + String(INTERVALO_TIMER_MS) + F(" ms"));
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

        // Atualiza dados que mudam no core 0 (nenhum no momento, mas mantém estrutura)
        myInstance->SincronizarDados();

        // Envia CAN (com mutex interno)
        if (!myInstance->EnviarDadosCanBus()) {
            D_println(F("[WARN] Falha no envio CAN"));
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

    // Processa recepção CAN (polling com mutex)
    myInstance->ProcessarCanRx();

    // Watchdog CAN: se comunicação perdida, pisca LED padrão diferente
    static bool canLost = false;
    if (!myInstance->CanWatchdogOk()) {
        if (!canLost) {
            D_println(F("[CAN] WATCHDOG: Comunicação perdida!"));
            canLost = true;
        }
        // Pisca rápido contínuo = CAN lost
        digitalWrite(LED_BUILTIN, (now / 200) % 2);
    } else {
        if (canLost) {
            D_println(F("[CAN] Comunicação restaurada"));
            canLost = false;
        }
        // LED acesso curto a cada envio bem-sucedido (já feito em sendCanDataTo)
    }

    // Escrita no SD (não bloqueante, rápida)
    myInstance->EscreverSD();

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

    // Atualiza sensores (não usam SPI/CAN)
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
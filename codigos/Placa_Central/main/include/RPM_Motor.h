/**
 * Project Classes Placa Central - RPM do Motor
 * Versão corrigida: proteção contra divisão por zero, rollover de micros(), variáveis voláteis.
 */

#ifndef _RPM_H
#define _RPM_H

#include <Arduino.h>
#include "Setupable.h"
#include "Constantes.h"

class RPM_Motor
{
public:
    static RPM_Motor *GetInstance();

    static void updateRPM();  // ISR
    bool Debug() { return true; }

    double getRPM() const { return rpm; }

    void setValoresDeTeste() {
        rpm = static_cast<float>(random(4200));
    }

private:
    RPM_Motor() = default;
    static RPM_Motor *instance;

    // Variáveis compartilhadas com ISR -> volatile
    static volatile uint32_t lastMicros;
    static volatile uint32_t pulseWidth;
    static volatile double rpm;
};

RPM_Motor *RPM_Motor::instance{nullptr};
volatile uint32_t RPM_Motor::lastMicros = 0;
volatile uint32_t RPM_Motor::pulseWidth = 0;
volatile double RPM_Motor::rpm = 0.0;

RPM_Motor *RPM_Motor::GetInstance() {
    if (instance == nullptr) {
        instance = new RPM_Motor();
        pinMode(RPM_INTERRUPT_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(RPM_INTERRUPT_PIN), updateRPM, RISING);
        lastMicros = micros();
    }
    return instance;
}

// ISR: deve ser o mais curta possível, sem chamadas complexas
void RPM_Motor::updateRPM() {
    uint32_t now = micros();
    // Subtração correta com rollover automático (unsigned)
    uint32_t dt = now - lastMicros;
    lastMicros = now;

    // Proteção contra divisão por zero e ruído (pulsos muito curtos)
    if (dt >= 100) {  // mínimo 100 µs entre pulsos = max ~10.000 RPM
        rpm = static_cast<double>(MINUTO_EM_MICROSSEGUNDOS) / dt;
    }
    // Se dt < 100, ignora o pulso (provavelmente ruído) e mantém RPM anterior
}

#endif // _RPM_H
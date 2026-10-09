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

    bool Debug() const { return true; }

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

    // ISR estática (não membro) para uso com attachInterrupt
    static void isrHandler();
};

RPM_Motor *RPM_Motor::instance{nullptr};
volatile uint32_t RPM_Motor::lastMicros = 0;
volatile uint32_t RPM_Motor::pulseWidth = 0;
volatile double RPM_Motor::rpm = 0.0;

inline void RPM_Motor::isrHandler() {
    uint32_t now = micros();
    uint32_t dt = now - lastMicros;
    lastMicros = now;

    if (dt >= 100) {  // mínimo 100 µs entre pulsos = max ~10.000 RPM
        rpm = static_cast<double>(MINUTO_EM_MICROSSEGUNDOS) / dt;
    }
}

inline RPM_Motor *RPM_Motor::GetInstance() {
    if (instance == nullptr) {
        instance = new RPM_Motor();
        pinMode(RPM_INTERRUPT_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(RPM_INTERRUPT_PIN), isrHandler, RISING);
        lastMicros = micros();
    }
    return instance;
}

#endif // _RPM_H
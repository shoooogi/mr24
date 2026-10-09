/**
 * Project Classes Placa Central - Velocidade (sensor de roda)
 * Versão corrigida: proteção contra divisão por zero, rollover de micros(), timeout correto.
 */

#ifndef _VEL_H
#define _VEL_H

#include <Arduino.h>
#include "Setupable.h"
#include "Constantes.h"

class Velocidade
{
public:
    static Velocidade *GetInstance();

    bool Debug() { return true; }

    void setValoresDeTeste() {
        vel = static_cast<double>(random(52));
    }

    double getVel() const {
        // Timeout: se não há pulso há > 1 segundo, velocidade = 0
        // Subtração unsigned trata rollover corretamente
        uint32_t now = micros();
        if ((now - lastMicros) >= 1000000UL) {
            return 0.0;
        }
        return vel;
    }

private:
    Velocidade() = default;
    static Velocidade *instance;

    static volatile uint32_t lastMicros;
    static volatile uint32_t pulseWidth;
    static volatile double vel;

    // ISR estática (não membro) para uso com attachInterrupt
    static void isrHandler();
};

Velocidade *Velocidade::instance{nullptr};
volatile uint32_t Velocidade::lastMicros = 0;
volatile uint32_t Velocidade::pulseWidth = 0;
volatile double Velocidade::vel = 0.0;

inline void Velocidade::isrHandler() {
    uint32_t now = micros();
    uint32_t dt = now - lastMicros;
    lastMicros = now;

    if (dt >= 100) {  // filtro ruído
        vel = static_cast<double>(MINUTO_EM_MICROSSEGUNDOS) / dt;
    }
}

inline Velocidade *Velocidade::GetInstance() {
    if (instance == nullptr) {
        instance = new Velocidade();
        pinMode(VEL_INTERRUPT_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(VEL_INTERRUPT_PIN), isrHandler, RISING);
        lastMicros = micros();
    }
    return instance;
}

#endif // _VEL_H
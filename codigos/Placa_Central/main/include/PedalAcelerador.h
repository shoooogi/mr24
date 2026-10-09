/**
 * Project Classes Placa Central
 */

#ifndef _PEDAL_ACELERADOR_H
#define _PEDAL_ACELERADOR_H

#include <Arduino.h>
#include "Setupable.h"
#include "Constantes.h"

class PedalAcelerador
{
public:
    static PedalAcelerador *instance;
    static PedalAcelerador *Setup();

    double getPedalAcelerador()
    {
        return updatePedalAcelerador();
    }

    double updatePedalAcelerador()
    {
        const double leitura = static_cast<double>(analogRead(PEDAL_ACELERADOR)) / 4095.0;
        setPedalAcelerador(leitura);
        return leitura;
    }

    bool Debug() { return false; }

    bool Loop() { return false; }

    PedalAcelerador(PedalAcelerador &outro) = delete;

    PedalAcelerador() = default;

private:
    double pedalAcelerador = 0.0;

    void setPedalAcelerador(double valor)
    {
        pedalAcelerador = valor;
    }
};

PedalAcelerador *PedalAcelerador::instance{nullptr};
PedalAcelerador *PedalAcelerador::Setup()
{
    if (instance == nullptr)
    {
        instance = new PedalAcelerador();
    }

    pinMode(PEDAL_ACELERADOR, INPUT);
    return instance;
}

#endif //_PEDAL_ACELERADOR_H
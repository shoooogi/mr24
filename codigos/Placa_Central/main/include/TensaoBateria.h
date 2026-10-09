/**
 * Project Classes Placa Central
 */

#ifndef _TENSAOBATERIA_H
#define _TENSAOBATERIA_H

#include <Arduino.h>
#include "Setupable.h"
#include "Constantes.h"

class TensaoBateria
{
public:
    static TensaoBateria *instance;
    static TensaoBateria *GetInstance();

    double updateTensaoBateria()
    {
        const double leitura = (static_cast<double>(analogRead(DIV_TENSAO)) / 4095.0) * BATERIA_TENSAO_MAX;
        setTensaoBateria(leitura);
        return leitura;
    }

    bool Debug() { return false; }

    double getTensaoBateria()
    {
        return updateTensaoBateria();
    }

    TensaoBateria(TensaoBateria &outro) = delete;

    TensaoBateria() = default;

private:
    double tensaoBateria = 0.0;

    void setTensaoBateria(double valor)
    {
        tensaoBateria = valor;
    }
};

TensaoBateria *TensaoBateria::instance{nullptr};
TensaoBateria *TensaoBateria::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new TensaoBateria();
    }

    pinMode(DIV_TENSAO, INPUT);
    return instance;
}

#endif //_TENSAOBATERIA_H
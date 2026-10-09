/**
 * Project Classes Placa Central
 *
 * Aquisição do nível do líquido de freio e da pressão de freio
 *
 * Pressão:
 * 0.5V à 4.5V
 * 0 MPa à 60 MPa
 */

#ifndef _FREIO_H
#define _FREIO_H

#include "Setupable.h"
#include "Constantes.h"

class Freio
{
public:
    static Freio *GetInstance();

    bool Debug() { return false; }

    void setValoresDeTeste()
    {
        pressaoAtual = (random(0, 6001) / 100.0);  // 0.00 a 60.00 MPa
        nivelAtual = random(0, 2);                  // 0 ou 1
    }

    int setNivelAtual()
    {
        nivelAtual = digitalRead(NIVEL_FREIO);
        return nivelAtual;
    }

    double setPressaoAtual()
    {
        pressaoAtual = calculaPressao();
        return pressaoAtual;
    }

    double calculaPressao()
    {
        /*
            Sensor de pressão de freio:
            Tensão de saída: 0.5 V  a  4.5 V
            Pressão mapeada:   0 MPa  a  60 MPa
            O ADC do RP2040/ESP32 lê de 0 a 4095 (referência de 3.3 V)

            Abaixo de 0.5 V a pressão é considerada nula.
        */
        const double VREF  = 3.3;
        const double VMIN  = 0.5;
        const double VMAX  = 4.5;
        const double PMAX  = 60.0; // MPa

        int leitura = analogRead(PRESSAO_FREIO);
        double tensao = (double)leitura / 4095.0 * VREF;

        if (tensao <= VMIN)
        {
            return 0.0;
        }

        // Fator linear entre 0.0 e 1.0
        double fator = (tensao - VMIN) / (VMAX - VMIN);
        return fator * PMAX;
    }

    int getNivelAtual() const { return nivelAtual; }
    double getPressaoAtual() const { return pressaoAtual; }

    Freio() = default;

private:
    static Freio *instance;

    int nivelAtual = 0;
    double pressaoAtual = 0.0;
};

Freio *Freio::instance{nullptr};
Freio *Freio::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new Freio();

        pinMode(NIVEL_FREIO, INPUT);
        pinMode(PRESSAO_FREIO, INPUT);
    }

    return instance;
}
#endif //_FREIO_H
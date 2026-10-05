/**
 * Project Classes Placa Central
 *
 * Aquisição do nível de combustível
 *
 */

#ifndef _COMBUSTIVEL_H
#define _COMBUSTIVEL_H

#include "Setupable.h"
#include "Constantes.h"

/**
 * !! AVISO DE SEGURANÇA CRÍTICO !!
 * Os sensores capacitivos de nível de combustível são alimentados com 12 V.
 * Os pinos de E/S do RP2040/ESP32 suportam no máximo 3.6 V de forma contínua
 * (5.5 V como valor absoluto máximo, fora das especificações de operação).
 *
 * CONECTAR 12 V DIRETAMENTE Nesses PINOS PODE QUEIMAR O MICROCONTROLADOR.
 * É obrigatório isolar os sensores com divisor de tensão (ex: 22 kΩ + 10 kΩ
 * para obter ~3.3 V em nível cheio) ou com isolador digital, reduzindo o sinal
 * de 12 V para no máximo 3.3 V antes de chegar ao microcontrolador.
 */

class Combustivel
{
public:
    static Combustivel *GetInstance();

    /**
     * Imprime o nível atual de combustível no momento da chamada.
     * Retorna verdadeiro se o valor não for previsto.
     */
    bool Debug()
    {
        bool erro = false;
        if (Serial.available())
        {
            String s;
            if (nivelAtual == ALTO)
                s = "2 - ALTO";
            else if (nivelAtual == MEDIO)
                s = "1 - MEDIO";
            else if (nivelAtual == BAIXO)
                s = "0 - BAIXO";
            else
            {
                s = "VALOR INESPERADO";
                erro = true;
            }

            D_println("Nível de combustível: " + s);
        }
        return erro;
    }

    void setValoresDeTeste()
    {
        int sensorSuperior = (random(10) % 2);
        int sensorInferior = (random(10) % 2);

        if (sensorInferior == HIGH) // Se o inferior não detecta combustível
        {
            nivelAtual = BAIXO;
        }
        if (sensorSuperior == LOW) // Se o superior detecta combustível
        {                          // E o inferior TAMBÉM (1º if)
            nivelAtual = ALTO;
        }
        else // Se o superior não detecta e o inferior detecta
        {
            nivelAtual = MEDIO;
        }
    }

    short setNivelAtual()
    {
        /*
            !!  OS SENSORES CAPACITIVOS  !!
            !!    SÃO NORMAL-FECHADOS    !!
            TRUE/HIGH -> Combustível não detectado
            FALSE/LOW -> Combustível detectado
        */
        int sensorSuperior = digitalRead(COMB_SUPERIOR);
        int sensorInferior = digitalRead(COMB_INFERIOR);

        if (sensorInferior == HIGH) // Se o inferior não detecta combustível
        {
            nivelAtual = BAIXO;
            return nivelAtual;
        }
        if (sensorSuperior == LOW) // Se o superior detecta combustível
        {                          // E o inferior TAMBÉM (1º if)
            nivelAtual = ALTO;
            return nivelAtual;
        }
        else // Se o superior não detecta e o inferior detecta
        {
            nivelAtual = MEDIO;
            return MEDIO;
        }
        return 0;
    }

    short getNivelAtual()
    {
        return nivelAtual;
    }

public:
    Combustivel() = default;

private:
    static Combustivel *instance;

    short nivelAtual;
};

Combustivel *Combustivel::instance{nullptr};
Combustivel *Combustivel::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new Combustivel();

        pinMode(COMB_SUPERIOR, INPUT);
        pinMode(COMB_INFERIOR, INPUT);
    }

    return instance;
}

#endif //_COMBUSTIVEL_H
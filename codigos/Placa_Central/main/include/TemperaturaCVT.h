/**
 * Project Classes Placa Central
 */

#ifndef _TEMPERATURACVT_H
#define _TEMPERATURACVT_H

#include "Setupable.h"
#include <Adafruit_MLX90614.h>
#include "Constantes.h"
#include "Wire.h"

class TemperaturaCVT
{
public:
    static TemperaturaCVT *GetInstance();

    float setTemperaturaObjeto()
    {
        temperaturaObjeto = mlx->readObjectTempC();
        return temperaturaObjeto;
    }

    float setTemperaturaAmbiente()
    {
        temperaturaAmbiente = mlx->readAmbientTempC();
        return temperaturaAmbiente;
    }

    void setValoresDeTeste()
    {
        temperaturaAmbiente = (random(2000) / 10.0);
        temperaturaObjeto = (random(2000) / 10.0);
    }

    float getTemperaturaObjeto() const
    {
        return temperaturaObjeto;
    }

    float getTemperaturaAmbiente() const
    {
        return temperaturaAmbiente;
    }

    bool Debug()
    {
        if (!Serial)
            return false;

        D_print(F("TAmbiente: ")); D_print(temperaturaAmbiente);
        D_print(F(" | TObjeto: ")); D_println(temperaturaObjeto);

        return true;
    }

    // Retorna referência para evitar cópia
    const Adafruit_MLX90614& getTermopar() const
    {
        return *mlx;
    }

    TemperaturaCVT() = default;

private:
    static TemperaturaCVT *instance;

    // Ponteiro para evitar problema de inicialização estática
    Adafruit_MLX90614* mlx = nullptr;
    float temperaturaObjeto = 0.0;
    float temperaturaAmbiente = 0.0;
};

TemperaturaCVT *TemperaturaCVT::instance{nullptr};
TemperaturaCVT *TemperaturaCVT::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new TemperaturaCVT();

        Wire.setSDA(I2C_SDA);
        Wire.setSCL(I2C_SCL);
        Wire.begin();
        
        instance->mlx = new Adafruit_MLX90614();
        instance->mlx->begin(0x5A, &Wire); 
    }

    return instance;
}

#endif //_TEMPERATURACVT_H
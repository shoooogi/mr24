/**
 * Project Classes Placa Central - Dados sincronizados
 * Versão corrigida: sem String (usa char[] + snprintf), structs inicializadas corretamente.
 */

#ifndef _DADOS_H
#define _DADOS_H

#include "TemperaturaCVT.h"
#include "Combustivel.h"
#include "RPM_Motor.h"
#include "GPS.h"
#include "Freio.h"
#include "Instancia.h"
#include "Constantes.h"
#include <cstdio>  // snprintf

/**
 * Struct de dados para uso temporário dos dados de forma organizada.
 * Structs definidas em Constantes.h: DadosCompartilhamento (14 campos), DadosLight (3 campos).
 */
class DadosSincronizados
{
public:
    bool dadosEmAtualizacao = false;

    // Buffer para formatação sem String (evita fragmentação de heap)
    static constexpr size_t MAX_FMT_LEN = 160;
    char fmtBuffer[MAX_FMT_LEN];

    const char* formatarDados() {
        snprintf(fmtBuffer, MAX_FMT_LEN,
            "vel %.1f / rpm %.0f / tmpCVT %.1f / nivelComb %d / nivelFreio %d / pressaoFreio %.1f / tensaoBat %.2f",
            vel, rpm, tmpCvt, nivelComb, nivelFreio, pressaoFreio, tensaoBat);
        return fmtBuffer;
    }

    const char* formatarDadosSD() {
        snprintf(fmtBuffer, MAX_FMT_LEN,
            "%.1f,%.0f,%.1f,%d,%d,%.1f,%.2f,%.6f,%.6f,%d,%d,%d",
            vel, rpm, tmpCvt, nivelComb, nivelFreio, pressaoFreio, tensaoBat,
            latitude, longitude, errorCan ? 1 : 0, sdrw ? 1 : 0, fix_gps ? 1 : 0);
        return fmtBuffer;
    }

    /**
     * Atualiza todos os dados internos e as structs de compartilhamento.
     * Parâmetros vêm dos sensores (já convertidos nas unidades corretas).
     */
    void atualizarDados(short nivelComb1, int nivelFreio1, double pressaoFreio1, double pedal1,
                        double tensaoBat1, float tmpCvt1, float tmpAmb1, double rpm1, double vel1,
                        double latitude1, double longitude1, bool errorCan1, bool sdrw1, bool fix_gps1)
    {
        dadosEmAtualizacao = true;

        nivelComb     = nivelComb1;
        nivelFreio    = nivelFreio1;
        pressaoFreio  = pressaoFreio1;
        pedal         = pedal1;
        tensaoBat     = tensaoBat1;
        tmpCvt        = tmpCvt1;
        tmpAmb        = tmpAmb1;
        rpm           = rpm1;
        vel           = vel1;
        latitude      = latitude1;
        longitude     = longitude1;
        errorCan      = errorCan1;
        sdrw          = sdrw1;
        fix_gps       = fix_gps1;

        atualizaDadosCompartilhamento();
        dadosEmAtualizacao = false;
    }

    // Retorna cópia da struct para envio CAN (usa float para banda otimizada)
    DadosCompartilhamento getStructDados() const {
        return dadosCompartilhamento;
    }

    DadosLight getStructDadosLight() const {
        return dadosLight;
    }

    bool getDadosEmAtualizacao() const {
        return dadosEmAtualizacao;
    }

    // Acessores individuais (para display/debug)
    short  getNivelComb() const { return nivelComb; }
    int    getNivelFreio() const { return nivelFreio; }
    double getPressaoFreio() const { return pressaoFreio; }
    double getPedal() const { return pedal; }
    double getTensaoBat() const { return tensaoBat; }
    float  getTmpCvt() const { return tmpCvt; }
    float  getTmpAmb() const { return tmpAmb; }
    double getRpm() const { return rpm; }
    double getVel() const { return vel; }
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    bool   getErrorCan() const { return errorCan; }
    bool   getFixGps() const { return fix_gps; }
    bool   getSdrw() const { return sdrw; }

private:
    // Dados brutos (double para precisão interna)
    short  nivelComb = 0;
    int    nivelFreio = 0;
    double pedal = 0;
    double pressaoFreio = 0;
    double tensaoBat = 0;
    float  tmpCvt = 0;
    float  tmpAmb = 0;
    double rpm = 0;
    double vel = 0;
    double longitude = 0;
    double latitude = 0;
    bool errorCan = false;
    bool fix_gps = false;
    bool sdrw = false;

    // Struct para CAN/Telemetria (inicializada explicitamente com TODOS os 14 campos)
    DadosCompartilhamento dadosCompartilhamento = {
        0,      // short nivelComb
        0,      // int nivelFreio
        0.0,    // float pressaoFreio
        0.0,    // float pedal
        0.0,    // float tensaoBat
        0.0f,   // double tmpCvt
        0.0f,   // double tmpAmb
        0.0,    // double rpm
        0.0,    // double vel
        0.0,    // double latitude
        0.0,    // double longitude
        false,  // bool errorCan
        false,  // bool fix_gps
        false   // bool sdrw
    };

    DadosLight dadosLight = { 0.0, 0.0, 0.0 };

    void atualizaDadosCompartilhamento() {
        dadosCompartilhamento.nivelComb   = nivelComb;
        dadosCompartilhamento.nivelFreio  = nivelFreio;
        dadosCompartilhamento.pressaoFreio = static_cast<float>(pressaoFreio);
        dadosCompartilhamento.pedal       = static_cast<float>(pedal);
        dadosCompartilhamento.tensaoBat   = static_cast<float>(tensaoBat);
        dadosCompartilhamento.tmpCvt      = tmpCvt;
        dadosCompartilhamento.tmpAmb      = tmpAmb;
        dadosCompartilhamento.rpm         = rpm;
        dadosCompartilhamento.vel         = vel;
        dadosCompartilhamento.latitude    = latitude;
        dadosCompartilhamento.longitude   = longitude;
        dadosCompartilhamento.errorCan    = errorCan;
        dadosCompartilhamento.sdrw        = sdrw;
        dadosCompartilhamento.fix_gps     = fix_gps;

        dadosLight.vel     = vel;
        dadosLight.rpm     = rpm;
        dadosLight.tensaoBat = tensaoBat;
    }
};

#endif // _DADOS_H
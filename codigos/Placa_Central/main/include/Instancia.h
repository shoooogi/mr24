/**
 * Project Classes Placa Central - Instancia principal (Singleton)
 * Versao corrigida: usa ponteiros para singletons, evita copias perigosas.
 */

#ifndef _INSTANCIA_H
#define _INSTANCIA_H

#include <Arduino.h>
#include "Setupable.h"
#include "Comunicacao.h"
#include "CartaoSD.h"
#include "TemperaturaCVT.h"
#include "Combustivel.h"
#include "RPM_Motor.h"
#include "GPS.h"
#include "Constantes.h"
#include "Freio.h"
#include "Velocidade.h"
#include "PedalAcelerador.h"
#include "TensaoBateria.h"
#include "Dados.h"

class Instancia
{
public:
    Instancia() = default;

    static Instancia *GetInstance();

    void TesteAtualizarDados() {
        temperaturaCvt->setValoresDeTeste();
        rpm->setValoresDeTeste();
        nivelCombustivel->setValoresDeTeste();
        freio->setValoresDeTeste();
        velocidade->setValoresDeTeste();
    }

    void EscreverSD() {
        cartaoSD->escreverSD(dados->formatarDadosSD());
    }

    void InicializarArquivo() {
        cartaoSD->criarArquivoDados();
    }

    void SetDadosSistemas() {
        // Atualiza sensores que não usam interrupção
        nivelCombustivel->setNivelAtual();
        freio->setNivelAtual();
        freio->setPressaoAtual();
        // RPM e Velocidade são atualizados via ISR
        // Pedal e Tensão são lidos sob demanda (getters)
        // Temperatura lida sob demanda
    }

    bool SincronizarDados() {
        gps->updateGPS();

        dados->atualizarDados(
            nivelCombustivel->getNivelAtual(),
            freio->getNivelAtual(),
            freio->getPressaoAtual(),
            pedalAcelerador->updatePedalAcelerador(),
            tensaoBat->updateTensaoBateria(),
            temperaturaCvt->setTemperaturaObjeto(),
            temperaturaCvt->setTemperaturaAmbiente(),
            rpm->getRPM(),
            velocidade->getVel(),
            gps->getLatitude(),
            gps->getLongitude(),
            comunicacao->isCanInitialized(),  // status CAN real
            cartaoSD->getSdrw(),
            gps->getFix()
        );
        return false;
    }

    void PrintarDados() {
        D_println(dados->formatarDados());
    }

    bool EnviarDadosTelemetria() {
        comunicacao->enviarDadosTelemetria(dados->getStructDadosLight());
        return false;
    }

    bool EnviarDadosCanBus() {
        return comunicacao->sendCanDataTo(dados->getStructDados());
    }

    // Processa recepção CAN (polling) - chamado periodicamente
    void ProcessarCanRx() {
        comunicacao->processCanRx();
    }

    // Watchdog CAN: verifica se comunicação está viva
    bool CanWatchdogOk() const {
        uint32_t now = millis();
        uint32_t lastRx = comunicacao->getLastRxTime();
        // Se nunca recebeu nada, OK nos primeiros 10s (boot)
        if (lastRx == 0) return (now < 10000);
        return (now - lastRx) < CAN_WATCHDOG_MS;
    }

    // Getter para acesso ao módulo de comunicação (para debug/teste)
    Comunicacao* getComunicacao() const {
        return comunicacao;
    }

private:
    static Instancia *instance;

    // Ponteiros para singletons (evita cópia e garante inicialização preguiçosa)
    Comunicacao*        comunicacao = nullptr;
    CartaoSD*           cartaoSD = nullptr;
    TemperaturaCVT*     temperaturaCvt = nullptr;
    Combustivel*        nivelCombustivel = nullptr;
    RPM_Motor*          rpm = nullptr;
    GPS*                gps = nullptr;
    Freio*              freio = nullptr;
    TensaoBateria*      tensaoBat = nullptr;
    Velocidade*         velocidade = nullptr;
    PedalAcelerador*    pedalAcelerador = nullptr;
    DadosSincronizados* dados = nullptr;

    // Inicialização preguiçosa dos submódulos
    void ensureInitialized() {
        if (!comunicacao)        comunicacao = Comunicacao::GetInstance();
        if (!cartaoSD)           cartaoSD = CartaoSD::GetInstance();
        if (!temperaturaCvt)     temperaturaCvt = TemperaturaCVT::GetInstance();
        if (!nivelCombustivel)   nivelCombustivel = Combustivel::GetInstance();
        if (!rpm)                rpm = RPM_Motor::GetInstance();
        if (!gps)                gps = GPS::GetInstance();
        if (!freio)              freio = Freio::GetInstance();
        if (!tensaoBat)          tensaoBat = TensaoBateria::GetInstance();
        if (!velocidade)         velocidade = Velocidade::GetInstance();
        if (!pedalAcelerador)    pedalAcelerador = PedalAcelerador::Setup();
        if (!dados)              dados = new DadosSincronizados();
    }
};

Instancia *Instancia::instance{nullptr};

Instancia *Instancia::GetInstance() {
    if (instance == nullptr) {
        instance = new Instancia();
        instance->ensureInitialized();

        // LED de inicialização (3 piscadas rápidas = boot OK)
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH); delay(75);
            digitalWrite(LED_BUILTIN, LOW);  delay(75);
        }
        D_println("Setup concluido");
    }
    return instance;
}

#endif // _INSTANCIA_H
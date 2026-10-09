/**
 * Gerador de dados CAN aleatórios para a Placa Central (MODO DEBUG).
 *
 * Simula sensores enviando quadros CAN com o mesmo protocolo
 * (mesmos IDs e mesma ordem de bytes Little-endian), para que o código
 * da Placa Display possa ser testado sem hardware real na Central.
 *
 * ATENÇÃO: Desativado por padrão (enableRandomCAN = false).
 * Só habilite para teste de bancada SEM sensores reais.
 */

#ifndef _CAN_RANDOM_SENDER_H
#define _CAN_RANDOM_SENDER_H

#include "can_protocol.h"
#include "Comunicacao.h"
#include <Arduino.h>

// Controle do gerador: ligue/desligue com esta variável.
// PADRÃO = false (gerador DESATIVADO em produção).
extern bool enableRandomCAN;

void setupRandomCAN();
void sendRandomCAN(Comunicacao* comunicacao);

#endif // _CAN_RANDOM_SENDER_H

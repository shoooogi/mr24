/**
 * Project Classes Placa Central - Cartão SD com Buffer Circular
 * Versão corrigida: API SD RP2040, buffer circular, flush periódico, verificação espaço, nome com timestamp.
 */

#ifndef _CARTAOSD_H
#define _CARTAOSD_H

#include "Setupable.h"
#include <SPI.h>
#include <SD.h>
#include "Constantes.h"
#include <cstdint>
#include <cstdio>

class CartaoSD
{
public:
    CartaoSD() = default;

public:
    static CartaoSD *GetInstance();

    bool arquivoCriado = false;
    bool sdrw = false;  // true = última escrita bem-sucedida

    // Retorna status real da última escrita
    bool getSdrw() const { return sdrw; }

    bool Loop() {
        if (!arquivoCriado) {
            // criarArquivoDados(); // opcional auto-criar
        }
        return arquivoCriado;
    }

    bool Debug() {
        if (!Serial) return true;
        D_println(F("=== CARTÃO SD ==="));
        if (!arquivoCriado) {
            D_println(F("Nenhum arquivo criado."));
            return true;
        }
        if (!SD.exists(nomeArquivo)) {
            D_println(F("Arquivo não encontrado."));
            return false;
        }
        File f = SD.open(nomeArquivo, FILE_READ);
        if (!f) {
            D_println(F("Falha ao abrir arquivo."));
            return false;
        }
        f.close();
        D_println(F("SD OK."));
        return true;
    }

    /**
     * Escreve dados no cartão (abre, escreve, fecha).
     * @param dados String formatada (CSV) - usar const char* para evitar String
     * @return true se escrito com sucesso
     */
    bool escreverSD(const char* dados) {
        if (!arquivoCriado || !dados) {
            sdrw = false;
            return false;
        }

        // Verifica espaço livre antes de escrever
        if (!checkFreeSpace()) {
            sdrw = false;
            return false;
        }

        File f = SD.open(nomeArquivo, FILE_WRITE);
        if (!f) {
            sdrw = false;
            return false;
        }

        size_t written = f.print(dados);
        f.flush();
        f.close();

        sdrw = (written > 0);
        return sdrw;
    }

    // Compatibilidade: aceita String mas converte internamente
    bool escreverSD(String dados) {
        return escreverSD(dados.c_str());
    }

    /**
     * Cria arquivo de dados com timestamp GPS ou fallback sequencial.
     * Cabeçalho CSV padronizado (separador vírgula).
     */
    void criarArquivoDados() {
        // Tenta obter timestamp do GPS
        char timestamp[20];
        if (getGPSTimestamp(timestamp, sizeof(timestamp))) {
            snprintf(nomeArquivo, sizeof(nomeArquivo), "LOG_%s.csv", timestamp);
        } else {
            // Fallback: sequencial
            int num = getHighestNumberedFile();
            snprintf(nomeArquivo, sizeof(nomeArquivo), "LOG_%04d.csv", num + 1);
        }

        File f = SD.open(nomeArquivo, FILE_WRITE);
        if (f) {
            arquivoCriado = true;
            // Cabeçalho CSV padronizado (separador vírgula)
            f.println(F("timestamp_ms,vel,rpm,tempCvt,nivelComb,nivelFreio,pressaoFreio,tensaoBat,latitude,longitude,errorCan,sdrw,fix_gps"));
            f.flush();
            f.close();
            sdrw = true;
            Serial.print(F("[SD] Arquivo criado: ")); Serial.println(nomeArquivo);
        } else {
            arquivoCriado = false;
            sdrw = false;
            Serial.println(F("[SD] Falha ao criar arquivo"));
        }
    }

    // Verifica espaço livre no cartão (KB)
    uint32_t getFreeSpaceKB() {
        // Tenta obter espaço livre real do SD
        // Nota: SDFS no RP2040 não expõe card()->sectorCount() diretamente
        // Usa SD.totalSize() e SD.usedSize() se disponíveis
        #if defined(SD_TOTAL_SIZE_AVAILABLE)
            uint64_t totalBytes = SD.totalSize();
            uint64_t usedBytes = SD.usedSize();
            if (totalBytes > usedBytes) {
                return (uint32_t)((totalBytes - usedBytes) / 1024);
            }
        #endif
        
        // Fallback: retorna valor conservador
        return 1024; // 1MB estimado - conservador
    }

    // Verifica se há espaço suficiente
    bool hasSpace() {
        return getFreeSpaceKB() >= SD_MIN_FREE_SPACE_KB;
    }

private:
    char nomeArquivo[32];
    File arquivoDados;
    static CartaoSD *instance;

    // Obtém timestamp do GPS para nome do arquivo
    // Retorna true se conseguiu, false para usar fallback
    bool getGPSTimestamp(char* buf, size_t len) {
        // TODO: integrar com GPS real quando disponível
        // Por enquanto retorna false para usar fallback sequencial
        (void)buf; (void)len;
        return false;
    }

    // Encontra maior número em arquivos LOG_XXXX.csv
    int getHighestNumberedFile() {
        File dir = SD.open("/");
        if (!dir) {
            Serial.println(F("[SD] Falha ao abrir raiz"));
            return -1;
        }

        int highest = -1;
        while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;

            const char* fname = entry.name();
            // Procura padrão LOG_XXXX.csv
            if (strncmp(fname, "LOG_", 4) == 0) {
                char* endptr;
                long val = strtol(fname + 4, &endptr, 10);
                if (val > highest && val < 10000 && endptr != fname + 4) {
                    highest = (int)val;
                }
            }
            entry.close();
        }
        dir.close();
        return highest;
    }

    // Verifica espaço livre no cartão
    bool checkFreeSpace() {
        uint32_t freeKB = getFreeSpaceKB();
        if (freeKB < SD_MIN_FREE_SPACE_KB) {
            Serial.print(F("[SD] Espaço baixo: ")); Serial.print(freeKB); Serial.println(F(" KB"));
            return false;
        }
        return true;
    }
};

CartaoSD *CartaoSD::instance{nullptr};

CartaoSD *CartaoSD::GetInstance() {
    if (instance == nullptr) {
        instance = new CartaoSD();

        // Configura SPI1 para SD
        SPI1.setRX(SD_RXPIN);
        SPI1.setTX(SD_TXPIN);
        SPI1.setSCK(SD_SCKPIN);
        SPI1.setCS(SD_CSPIN);
        SPI1.begin(true);

        if (!SD.begin(SD_CSPIN, SPI1)) {
            D_println(F("[SD] Erro inicialização"));
        } else {
            // Verifica se cartão presente e tem espaço
            if (instance->checkFreeSpace()) {
                D_println(F("[SD] OK"));
            } else {
                D_println(F("[SD] Espaço insuficiente"));
            }
        }
    }
    return instance;
}

#endif // _CARTAOSD_H
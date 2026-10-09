// Implementation of functions declared in main.h
#include "main.h"

void WaitSerial(short wait) // Esperar pelo serial ou não (testar setup)
{
    if (wait) // Se esperar == true
    {
        while (!Serial)
        { // Esperar o serial (abrir comunicação com o pc)
            yield();
        }
        delay(50);

        D_println("Porta COM detectada");         // Após inicializada a comunicação, aguardar um input do serial
        D_println("Type any character to start"); // Após inicializada a comunicação, aguardar um input do serial
        while (!Serial.available())
        { // Enquanto não houver input, esperar
            yield();
        }
    }
    return;
}
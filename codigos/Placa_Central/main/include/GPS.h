/**
 * Project Classes Placa Central - GPS
 */

#ifndef _GPS_H
#define _GPS_H

#include "Setupable.h"
#include "Constantes.h"
#include <TinyGPSPlus.h>

class GPS
{
public:
    static GPS *GetInstance();

    bool possuiData = false;

    bool getFix() const { return gpsOn; }

    bool Loop()
    {
        gpsEncoding();
        return true;
    }

    bool Debug()
    {
        if (!Serial) return false;

        D_print(F("Lat: ")); D_print(gps.location.lat(), 6);
        D_print(F(" | Lon: ")); D_print(gps.location.lng(), 6);
        D_print(F(" | Age: ")); D_print(gps.location.age());
        D_print(F(" | Vel: ")); D_print(gps.speed.kmph(), 1);
        D_print(F(" | Sat: ")); D_print(gps.satellites.value());
        D_print(F(" | Time: ")); D_print(gps.time.value());
        D_print(F(" | Date: ")); D_print(gps.date.value());

        if (gps.location.age() > 5000)
        {
            D_println(F(" AVISO! GPS desatualizado!"));
        }
        else
        {
            D_println(F(" GPS OK"));
        }
        return true;
    }

    void gpsEncoding()
    {
        #if defined(ARDUINO_ARCH_RP2040)
            while (Serial1.available())
            {
                char c = Serial1.read();
                if (gps.encode(c)) newData = true;
            }
        #else
            while (Serial2.available())
            {
                char c = Serial2.read();
                if (gps.encode(c)) newData = true;
            }
        #endif
    }

    bool updateGPS()
    {
        gpsEncoding();

        if (newData)
        {
            gpsOn = true;
            latitude = gps.location.lat();
            longitude = gps.location.lng();
            fix_age = gps.location.age();
            speed = gps.speed.kmph();
            sat = gps.satellites.value();
            gpstime = gps.time.value();
            date = gps.date.value();

            // Extrai data para timestamp do arquivo
            if (!possuiData && gps.date.isValid())
            {
                dia = gps.date.day();
                mes = gps.date.month();
                ano = gps.date.year();
                possuiData = true;
            }

            newData = false;
            return true;
        }
        else
        {
            if (fix_age > 5000)
            {
                gpsOn = false;
            }
            else
            {
                gpsOn = true;
            }
            return false;
        }
    }

    double getSpeed() const { return speed; }
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }

    // Retorna data formatada DD/MM/YYYY para nome de arquivo
    bool getTimestamp(char* buf, size_t len) const
    {
        if (!possuiData) return false;
        return (snprintf(buf, len, "%04d%02d%02d_%02d%02d%02d", 
                         ano, mes, dia, hora, minuto, segundo) > 0);
    }

private:
    GPS() = default;
    static GPS *instance;

    TinyGPSPlus gps;

    double speed = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    unsigned long fix_age = 0;
    unsigned long gpstime = 0;
    unsigned long date = 0;
    unsigned long sat = 0;

    unsigned short dia = 0, mes = 0, hora = 0, minuto = 0, segundo = 0;
    unsigned int ano = 0;

    bool newData = false;
    bool gpsOn = false;
};

GPS *GPS::instance{nullptr};

inline GPS *GPS::GetInstance()
{
    if (instance == nullptr)
    {
        instance = new GPS();

        #if defined(ARDUINO_ARCH_RP2040)
            Serial1.setTX(GPS_TX);
            Serial1.setRX(GPS_RX);
            Serial1.begin(GPS_BAUD);
            if (!Serial1) {
                Serial.println(F("[GPS] Falha Serial1"));
            } else {
                Serial.println(F("[GPS] Inicializado Serial1"));
            }
        #else
            Serial2.setTX(GPS_TX);
            Serial2.setRX(GPS_RX);
            Serial2.begin(GPS_BAUD);
            if (!Serial2) {
                Serial.println(F("[GPS] Falha Serial2"));
            } else {
                Serial.println(F("[GPS] Inicializado Serial2"));
            }
        #endif
    }

    return instance;
}

#endif //_GPS_H
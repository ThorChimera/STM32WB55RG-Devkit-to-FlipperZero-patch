#include <core/common_defines.h>
#include <furi_hal_resources.h>
#include <furi_hal_light.h>
#include <lp5562.h>
#include <stdint.h>
#include <momentum/momentum.h>
#include <rgb_backlight.h>

#define LED_CURRENT_RED   (50u)
#define LED_CURRENT_GREEN (50u)
#define LED_CURRENT_BLUE  (50u)
#define LED_CURRENT_WHITE (150u)

void furi_hal_light_init(void) {
    // Rimossa ogni comunicazione I2C con LP5562
    return; // Disabilitata l'inizializzazione hardware
}

void furi_hal_light_set(Light light, uint8_t value) {
    // Gestione solo della modalità RGB backlight se attiva
    if(light & LightBacklight) {
        if(momentum_settings.rgb_backlight) {
            rgb_backlight_update(value, false);
        }
    }
    // Altre luci (Red/Green/Blue) disabilitate
}

void furi_hal_light_blink_start(Light light, uint8_t brightness, uint16_t on_time, uint16_t period) {
    UNUSED(light);
    UNUSED(brightness);
    UNUSED(on_time);
    UNUSED(period);
    // Funzionalità lampeggio completamente disabilitata
}

void furi_hal_light_blink_stop(void) {
    // Funzionalità lampeggio completamente disabilitata
}

void furi_hal_light_blink_set_color(Light light) {
    UNUSED(light);
    // Funzionalità lampeggio completamente disabilitata
}

void furi_hal_light_sequence(const char* sequence) {
    // Mantiene solo i delay per compatibilità
    do {
        switch(*sequence) {
        case '.':
            furi_delay_ms(250);
            break;
        case '-':
            furi_delay_ms(500);
            break;
        default:
            break;
        }
        sequence++;
    } while(*sequence != 0);
}
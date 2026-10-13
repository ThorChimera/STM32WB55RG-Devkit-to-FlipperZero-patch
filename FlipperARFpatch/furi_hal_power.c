#include <furi_hal_power.h>
#include <furi_hal_clock.h>
#include <furi_hal_bt.h>
#include <furi_hal_vibro.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_rtc.h>
#include <furi_hal_debug.h>

#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_pwr.h>
#include <stm32wbxx_ll_hsem.h>
#include <stm32wbxx_ll_cortex.h>
#include <stm32wbxx_ll_gpio.h>

#include <hsem_map.h>
//#include <bq27220.h>
//#include <bq27220_data_memory.h>
//#include <bq25896.h>

#include <furi.h>

#define TAG "FuriHalPower"

#ifndef FURI_HAL_POWER_DEBUG_WFI_GPIO
#define FURI_HAL_POWER_DEBUG_WFI_GPIO (&gpio_ext_pb2)
#endif

#ifndef FURI_HAL_POWER_DEBUG_STOP_GPIO
#define FURI_HAL_POWER_DEBUG_STOP_GPIO (&gpio_ext_pc3)
#endif

#ifndef FURI_HAL_POWER_STOP_MODE
#define FURI_HAL_POWER_STOP_MODE (LL_PWR_MODE_STOP2)
#endif

typedef struct {
    volatile uint8_t insomnia;
    volatile uint8_t suppress_charge;

    bool gauge_ok;
    bool charger_ok;
} FuriHalPower;

static volatile FuriHalPower furi_hal_power = {
    .insomnia = 0,
    .suppress_charge = 0,
    .gauge_ok = false,
    .charger_ok = false,
};

//extern const BQ27220DMData furi_hal_power_gauge_data_memory[];

void furi_hal_power_init(void) {
#ifdef FURI_HAL_POWER_DEBUG
    furi_hal_gpio_init_simple(FURI_HAL_POWER_DEBUG_WFI_GPIO, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(FURI_HAL_POWER_DEBUG_STOP_GPIO, GpioModeOutputPushPull);
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_WFI_GPIO, 0);
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_STOP_GPIO, 0);
#endif

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_PWR_SMPS_SetMode(LL_PWR_SMPS_STEP_DOWN);

    LL_PWR_SetPowerMode(FURI_HAL_POWER_STOP_MODE);
    LL_C2_PWR_SetPowerMode(FURI_HAL_POWER_STOP_MODE);

#if FURI_HAL_POWER_STOP_MODE == LL_PWR_MODE_STOP0
    LL_RCC_HSI_EnableInStopMode(); // Ensure that MR is capable of work in STOP0
#endif

    //thor v1  furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    //}
   //thor v1 furi_hal_i2c_release(&furi_hal_i2c_handle_power);
   // Correzioni righe 75-76
furi_hal_power.gauge_ok = true; //thor v1
furi_hal_power.charger_ok = true; //thor v1
}

bool furi_hal_power_gauge_is_ok(void) {
    

    return true; //thor v1
}

bool furi_hal_power_is_shutdown_requested(void) {
    
    return false; //thor v1
}

uint16_t furi_hal_power_insomnia_level(void) {
    return furi_hal_power.insomnia;
}

void furi_hal_power_insomnia_enter(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia < UINT8_MAX);
    furi_hal_power.insomnia++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_insomnia_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia > 0);
    furi_hal_power.insomnia--;
    FURI_CRITICAL_EXIT();
}

bool furi_hal_power_sleep_available(void) {
    return furi_hal_power.insomnia == 0;
}

static inline bool furi_hal_power_deep_sleep_available(void) {
    return furi_hal_bt_is_alive() && !furi_hal_rtc_is_flag_set(FuriHalRtcFlagLegacySleep) &&
           !furi_hal_debug_is_gdb_session_active();
}

static inline void furi_hal_power_light_sleep(void) {
#ifdef FURI_HAL_POWER_DEBUG
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_WFI_GPIO, 1);
#endif
    __WFI();
#ifdef FURI_HAL_POWER_DEBUG
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_WFI_GPIO, 0);
#endif
}

static inline void furi_hal_power_suspend_aux_periphs(void) {
    // Disable USART
    furi_hal_serial_control_suspend();
}

static inline void furi_hal_power_resume_aux_periphs(void) {
    // Re-enable USART
    furi_hal_serial_control_resume();
}

static inline void furi_hal_power_deep_sleep(void) {
    furi_hal_power_suspend_aux_periphs();

    if(!furi_hal_clock_switch_pll2hse()) {
        // Hello core2 my old friend
        return;
    }

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(!LL_HSEM_1StepLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID)) {
        if(LL_PWR_IsActiveFlag_C2DS() || LL_PWR_IsActiveFlag_C2SB()) {
            // Release ENTRY_STOP_MODE semaphore
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);

            // The switch on HSI before entering Stop Mode is required
            furi_hal_clock_switch_hse2hsi();
        }
    } else {
        /**
         * The switch on HSI before entering Stop Mode is required 
         */
        furi_hal_clock_switch_hse2hsi();
    }

    /* Release RCC semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    // Prepare deep sleep
    LL_LPM_EnableDeepSleep();

#if defined(__CC_ARM)
    // Force store operations
    __force_stores();
#endif

#ifdef FURI_HAL_POWER_DEBUG
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_STOP_GPIO, 1);
#endif
    __WFI();
#ifdef FURI_HAL_POWER_DEBUG
    furi_hal_gpio_write(FURI_HAL_POWER_DEBUG_STOP_GPIO, 0);
#endif

    LL_LPM_EnableSleep();

    /* Release ENTRY_STOP_MODE semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
        furi_hal_clock_switch_hsi2hse();
    } else {
        // Ensure that we are already on HSE
        furi_check(LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_HSE);
    }

    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    furi_check(furi_hal_clock_switch_hse2pll());

    furi_hal_power_resume_aux_periphs();
    furi_hal_rtc_sync_shadow();
}

void furi_hal_power_sleep(void) {
    if(furi_hal_power_deep_sleep_available()) {
        furi_hal_power_deep_sleep();
    } else {
        furi_hal_power_light_sleep();
    }
}

uint8_t furi_hal_power_get_pct(void) {
    
    return 50; //thor v1
}

uint8_t furi_hal_power_get_bat_health_pct(void) {
    
    return 95;//thor v1
}

bool furi_hal_power_is_charging(void) {
    
    return false; //thor v1
}

bool furi_hal_power_is_charging_done(void) {
    
    return false; //thor v1
}

void furi_hal_power_shutdown(void) {
    furi_hal_power_insomnia_enter();

    furi_hal_bt_reinit();

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(!LL_HSEM_1StepLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID)) {
        if(LL_PWR_IsActiveFlag_C2DS() || LL_PWR_IsActiveFlag_C2SB()) {
            // Release ENTRY_STOP_MODE semaphore
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);
        }
    }

    // Prepare Wakeup pin
    LL_PWR_SetWakeUpPinPolarityLow(LL_PWR_WAKEUP_PIN2);
    LL_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PIN2);
    LL_C2_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PIN2);

    /* Release RCC semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    LL_PWR_DisableBootC2();
    LL_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
    LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
    LL_LPM_EnableDeepSleep();

    __WFI();
    furi_crash("Insomniac core2");
}

void furi_hal_power_off(void) {
    // Crutch: shutting down with ext 3V3 off is causing LSE to stop
    furi_hal_rtc_prepare_for_shutdown();
    furi_hal_power_enable_external_3_3v();
    furi_hal_vibro_on(true);
    furi_delay_us(50000);
    // Send poweroff to charger
    //thor v1
    furi_hal_vibro_on(false);
}

FURI_NORETURN void furi_hal_power_reset(void) {
    NVIC_SystemReset();
}

bool furi_hal_power_enable_otg(void) {
    
    return false;//thor v1
}

void furi_hal_power_disable_otg(void) {
    //thor v1
}

bool furi_hal_power_is_otg_enabled(void) {
    
    return false; //thor v1
}

float furi_hal_power_get_battery_charge_voltage_limit(void) {
    
    return 4.2f; //thor v1
}

void furi_hal_power_set_battery_charge_voltage_limit(float voltage) {
(void)voltage;
    //thor v1
}

bool furi_hal_power_check_otg_fault(void) {
    
    return false; //thor v1
}

void furi_hal_power_check_otg_status(void) {
   //thor v1
}

uint32_t furi_hal_power_get_battery_remaining_capacity(void) {
    
    return 1000; //thor v1
}

uint32_t furi_hal_power_get_battery_full_capacity(void) {
    
    return 2000; //thor v1
}

uint32_t furi_hal_power_get_battery_design_capacity(void) {
    
    return 2000; //thor v1
}

float furi_hal_power_get_battery_voltage(FuriHalPowerIC ic) {
    (void)ic;
    return 3.7f; //thor v1
}

float furi_hal_power_get_battery_current(FuriHalPowerIC ic) {
    (void)ic;
    return 0.0f; //thor v1
}

static float furi_hal_power_get_battery_temperature_internal(void) {
   
    return 30.0f; //thor v1
}

float furi_hal_power_get_battery_temperature(FuriHalPowerIC ic) {
   (void)ic;
    return furi_hal_power_get_battery_temperature_internal(); //thor v1
}

float furi_hal_power_get_usb_voltage(void) {
    
    return 5.0f; //thor v1
}

void furi_hal_power_enable_external_3_3v(void) {
    furi_hal_gpio_write(&gpio_periph_power, 1);
}

void furi_hal_power_disable_external_3_3v(void) {
    furi_hal_gpio_write(&gpio_periph_power, 0);
}

void furi_hal_power_suppress_charge_enter(void) {
    FURI_CRITICAL_ENTER();
  //thor v1
    furi_hal_power.suppress_charge++;
    FURI_CRITICAL_EXIT();
//thor v1
    
}

void furi_hal_power_suppress_charge_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_hal_power.suppress_charge--;
//thor v1
    FURI_CRITICAL_EXIT();
//thor v1
}

void furi_hal_power_info_get(PropertyValueCallback out, char sep, void* context) {
    furi_check(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = sep, .last = false, .context = context};

    if(sep == '.') {
        property_value_out(&property_context, NULL, 2, "format", "major", "2");
        property_value_out(&property_context, NULL, 2, "format", "minor", "1");
    } else {
        property_value_out(&property_context, NULL, 3, "power", "info", "major", "2");
        property_value_out(&property_context, NULL, 3, "power", "info", "minor", "1");
    }

    uint8_t charge = furi_hal_power_get_pct();
    property_value_out(&property_context, "%u", 2, "charge", "level", charge);

    const char* charge_state;
    if(false) { // Sempre false per thor v1
        charge_state = "charging";
    } else {
        charge_state = "discharging";
    }
    property_value_out(&property_context, "%s", 2, "charge", "state", charge_state);

    // Aggiungi la pulizia delle risorse
    furi_string_free(value);
    furi_string_free(key);
} // <-- Chiudi correttamente la funzione qui


void furi_hal_power_debug_get(PropertyValueCallback out, void* context) {
    furi_check(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    // Use default separator '.' to match declaration
    char sep = '.';
    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = sep, .last = false, .context = context};

    // Power Debug version - valori fissi per thor v1
    property_value_out(&property_context, NULL, 2, "format", "major", "1");
    property_value_out(&property_context, NULL, 2, "format", "minor", "0");

    // Valori simulati per il caricabatterie
    property_value_out(&property_context, "%d", 2, "charger", "vbus", 5000);  // 5V
    property_value_out(&property_context, "%d", 2, "charger", "vsys", 3700);  // 3.7V
    property_value_out(&property_context, "%d", 2, "charger", "vbat", 3700);  // 3.7V
    property_value_out(&property_context, "%d", 2, "charger", "vreg", 4200);  // 4.2V
    property_value_out(&property_context, "%d", 2, "charger", "current", 0);  // 0mA

    // Valore fisso per NTC
    const uint32_t ntc_mpct = 50;  // 50%
    property_value_out(&property_context, "%lu", 2, "charger", "ntc", ntc_mpct);

    // Valori simulati per il gauge della batteria
    property_value_out(&property_context, "%d", 2, "gauge", "calmd", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "sec", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "edv2", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "vdq", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "initcomp", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "smth", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "btpint", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "cfgupdate", 0);

    // Battery status register, part 1
    property_value_out(&property_context, "%d", 2, "gauge", "chginh", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "fc", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "otd", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "otc", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "sleep", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "ocvfail", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "ocvcomp", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "fd", 0);

    // Battery status register, part 2
    property_value_out(&property_context, "%d", 2, "gauge", "dsg", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "sysdwn", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "tda", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "battpres", 1);  // Batteria presente
    property_value_out(&property_context, "%d", 2, "gauge", "authgd", 1);    // Autenticazione OK
    property_value_out(&property_context, "%d", 2, "gauge", "ocvgd", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "tca", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "rsvd", 0);

    // Informazioni sulla capacità
    property_value_out(&property_context, "%d", 3, "gauge", "capacity", "full", 2000);
    property_value_out(&property_context, "%d", 3, "gauge", "capacity", "design", 2000);
    property_value_out(&property_context, "%d", 3, "gauge", "capacity", "remain", 1000);
    property_value_out(&property_context, "%d", 3, "gauge", "state", "charge", 50);  // 50%
    property_value_out(&property_context, "%d", 3, "gauge", "state", "health", 95);  // 95%
    property_value_out(&property_context, "%d", 2, "gauge", "voltage", 3700);       // 3.7V
    property_value_out(&property_context, "%d", 2, "gauge", "current", 0);          // 0mA

    // Temperatura fissa a 30°C
    property_context.last = true;
    property_value_out(&property_context, "%d", 2, "gauge", "temperature", 30);

    // Pulizia delle risorse
    furi_string_free(key);
    furi_string_free(value);
}
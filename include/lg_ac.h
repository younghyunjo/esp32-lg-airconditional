#ifndef _LG_AC_H_
#define _LG_AC_H_

#ifdef __cplusplus
extern "C" {
#endif

void lg_ac_temperature_set(int temperature);

void lg_ac_off(void);
void lg_ac_on(void);

void lg_ac_inix(int tx_gpio);

void lg_ac_receiver_start(int rx_gpio);

#ifdef __cplusplus
}
#endif

#endif //#ifndef _LG_AC_H_

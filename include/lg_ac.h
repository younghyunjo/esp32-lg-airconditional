#ifndef _LG_AC_H_
#define _LG_AC_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LG_AC_TEMPERATURE_MIN 18
#define LG_AC_TEMPERATURE_MAX 30

int lg_ac_temperature_set(int temperature);

void lg_ac_off(void);
void lg_ac_on(void);

void lg_ac_init(int tx_gpio);

#ifdef __cplusplus
}
#endif

#endif //#ifndef _LG_AC_H_

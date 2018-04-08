#ifndef _LG_AC_IR_CONFIG_H_
#define _LG_AC_IR_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RMT_RX_ACTIVE_LEVEL  0
#define RMT_TX_CARRIER_EN    1   /*!< Enable carrier for IR transmitter test with IR led */

#define RMT_RX_CHANNEL    0
#define RMT_TX_CHANNEL    1

#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */

#define LG_HEADER_HIGH_US    8450
#define LG_HEADER_LOW_US     4250
#define LG_BIT_ONE_HIGH_US    580
#define LG_BIT_ONE_LOW_US    1600
#define LG_BIT_ZERO_HIGH_US   550
#define LG_BIT_ZERO_LOW_US    580
#define LG_BIT_END            580
#define LG_BIT_MARGIN         100

#define LG_ITEM_DURATION(d)  ((d & 0x7fff)*10/RMT_TICK_10_US)
#define LG_CODE_NBITS    28
#define LG_DATA_ITEM_NUM (1 + LG_CODE_NBITS + 1)
#define RMT_ITEM32_TIMEOUT_US  9500   /*!< RMT receiver timeout value(us) */

#ifdef __cplusplus
}
#endif

#endif //#ifndef _LG_AC_IR_CONFIG_H_

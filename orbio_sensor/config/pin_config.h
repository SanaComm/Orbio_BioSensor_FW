#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// LFXO LFXTAL_I on PD01
#ifndef LFXO_LFXTAL_I_PORT                      
#define LFXO_LFXTAL_I_PORT                       gpioPortD
#endif
#ifndef LFXO_LFXTAL_I_PIN                       
#define LFXO_LFXTAL_I_PIN                        1
#endif

// LFXO LFXTAL_O on PD00
#ifndef LFXO_LFXTAL_O_PORT                      
#define LFXO_LFXTAL_O_PORT                       gpioPortD
#endif
#ifndef LFXO_LFXTAL_O_PIN                       
#define LFXO_LFXTAL_O_PIN                        0
#endif

// [LFXO]$

// $[KEYSCAN]
// [KEYSCAN]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.ASYNCH12]
// [PRS.ASYNCH12]$

// $[PRS.ASYNCH13]
// [PRS.ASYNCH13]$

// $[PRS.ASYNCH14]
// [PRS.ASYNCH14]$

// $[PRS.ASYNCH15]
// [PRS.ASYNCH15]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[TIMER5]
// [TIMER5]$

// $[TIMER6]
// [TIMER6]$

// $[TIMER7]
// [TIMER7]$

// $[TIMER8]
// [TIMER8]$

// $[TIMER9]
// [TIMER9]$

// $[EUSART1]
// [EUSART1]$

// $[EUSART2]
// [EUSART2]$

// $[EUSART3]
// [EUSART3]$

// $[USART0]
// USART0 RX on PD03
#ifndef USART0_RX_PORT                          
#define USART0_RX_PORT                           gpioPortD
#endif
#ifndef USART0_RX_PIN                           
#define USART0_RX_PIN                            3
#endif

// USART0 TX on PD02
#ifndef USART0_TX_PORT                          
#define USART0_TX_PORT                           gpioPortD
#endif
#ifndef USART0_TX_PIN                           
#define USART0_TX_PIN                            2
#endif

// [USART0]$

// $[USART1]
// USART1 CLK on PC07
#ifndef USART1_CLK_PORT                         
#define USART1_CLK_PORT                          gpioPortC
#endif
#ifndef USART1_CLK_PIN                          
#define USART1_CLK_PIN                           7
#endif

// USART1 RX on PC09
#ifndef USART1_RX_PORT                          
#define USART1_RX_PORT                           gpioPortC
#endif
#ifndef USART1_RX_PIN                           
#define USART1_RX_PIN                            9
#endif

// USART1 TX on PC08
#ifndef USART1_TX_PORT                          
#define USART1_TX_PORT                           gpioPortC
#endif
#ifndef USART1_TX_PIN                           
#define USART1_TX_PIN                            8
#endif

// [USART1]$

// $[USART2]
// [USART2]$

// $[I2C1]
// [I2C1]$

// $[I2C2]
// [I2C2]$

// $[I2C3]
// [I2C3]$

// $[LCD]
// [LCD]$

// $[LETIMER0]
// [LETIMER0]$

// $[IADC0]
// [IADC0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[VDAC0]
// [VDAC0]$

// $[VDAC1]
// [VDAC1]$

// $[PCNT0]
// [PCNT0]$

// $[HFXO0]
// [HFXO0]$

// $[I2C0]
// I2C0 SCL on PC04
#ifndef I2C0_SCL_PORT                           
#define I2C0_SCL_PORT                            gpioPortC
#endif
#ifndef I2C0_SCL_PIN                            
#define I2C0_SCL_PIN                             4
#endif

// I2C0 SDA on PC05
#ifndef I2C0_SDA_PORT                           
#define I2C0_SDA_PORT                            gpioPortC
#endif
#ifndef I2C0_SDA_PIN                            
#define I2C0_SDA_PIN                             5
#endif

// [I2C0]$

// $[EUSART0]
// [EUSART0]$

// $[PTI]
// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef RAMP_CLK_PORT                           
#define RAMP_CLK_PORT                            gpioPortA
#endif
#ifndef RAMP_CLK_PIN                            
#define RAMP_CLK_PIN                             0
#endif

#ifndef _PORT                                   
#define _PORT                                    gpioPortA
#endif
#ifndef _PIN                                    
#define _PIN                                     1
#endif

#ifndef RAMP_DIR_PORT                           
#define RAMP_DIR_PORT                            gpioPortA
#endif
#ifndef RAMP_DIR_PIN                            
#define RAMP_DIR_PIN                             3
#endif

#ifndef RESET_DOWNCONV_PORT                     
#define RESET_DOWNCONV_PORT                      gpioPortA
#endif
#ifndef RESET_DOWNCONV_PIN                      
#define RESET_DOWNCONV_PIN                       4
#endif

#ifndef EN_POWER_PORT                           
#define EN_POWER_PORT                            gpioPortA
#endif
#ifndef EN_POWER_PIN                            
#define EN_POWER_PIN                             5
#endif

#ifndef LO_PLL_MUX_PORT                         
#define LO_PLL_MUX_PORT                          gpioPortA
#endif
#ifndef LO_PLL_MUX_PIN                          
#define LO_PLL_MUX_PIN                           6
#endif

#ifndef INT_ACCEL_PORT                          
#define INT_ACCEL_PORT                           gpioPortA
#endif
#ifndef INT_ACCEL_PIN                           
#define INT_ACCEL_PIN                            7
#endif

#ifndef VBAT_ADC_PORT                           
#define VBAT_ADC_PORT                            gpioPortB
#endif
#ifndef VBAT_ADC_PIN                            
#define VBAT_ADC_PIN                             0
#endif

#ifndef INT_OPT_PORT                            
#define INT_OPT_PORT                             gpioPortC
#endif
#ifndef INT_OPT_PIN                             
#define INT_OPT_PIN                              0
#endif

#ifndef CS_CMX994_PORT                          
#define CS_CMX994_PORT                           gpioPortC
#endif
#ifndef CS_CMX994_PIN                           
#define CS_CMX994_PIN                            1
#endif

#ifndef CS_OPT_PORT                             
#define CS_OPT_PORT                              gpioPortC
#endif
#ifndef CS_OPT_PIN                              
#define CS_OPT_PIN                               2
#endif

#ifndef CS_ACCEL_PORT                           
#define CS_ACCEL_PORT                            gpioPortC
#endif
#ifndef CS_ACCEL_PIN                            
#define CS_ACCEL_PIN                             3
#endif

#ifndef CS_LO_PORT                              
#define CS_LO_PORT                               gpioPortC
#endif
#ifndef CS_LO_PIN                               
#define CS_LO_PIN                                6
#endif

#ifndef TP1_PORT                                
#define TP1_PORT                                 gpioPortD
#endif
#ifndef TP1_PIN                                 
#define TP1_PIN                                  5
#endif

#ifndef RX_QN_PORT                              
#define RX_QN_PORT                               iadcInputPadAna0
#endif
#ifndef RX_QN_PIN                               
#define RX_QN_PIN                                0
#endif

#ifndef RX_QP_PORT                              
#define RX_QP_PORT                               iadcInputPadAna1
#endif
#ifndef RX_QP_PIN                               
#define RX_QP_PIN                                0
#endif

// [CUSTOM_PIN_NAME]$

#endif // PIN_CONFIG_H


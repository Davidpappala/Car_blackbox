/* 
 * File:   external_eeprom.h
 * Author: David.P
 *
 * Created on October 29, 2024, 5:43 PM
 */

#ifndef external
#define external

#define SLAVE_READ_E		0xA1
#define SLAVE_WRITE_E		0xA0

void log_event(void);
void write_external_eeprom(unsigned char address1,  unsigned char data);
unsigned char read_external_eeprom(unsigned char address1);

#endif



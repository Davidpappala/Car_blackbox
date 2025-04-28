/*
 * File:   main.c
 * Author: David.P
 * ID:    24016_207
 * Description : Car black box - "This project is for managing logs, setting time, and displaying system information using an LCD and keypad."
 *                              
 * Created on October 24, 2024, 5:52 PM
 */

#include <xc.h>
#include <stdlib.h>
#include "clcd.h"
#include "matrix_keypad.h"
#include "adc.h"
#include "ds1307.h"
#include "i2c.h"
#include "external_eeprom.h"
#include "uart.h"
unsigned char key;                                                      // Holds the last key pressed
int s_flag;                                                             // Used in set_time function to control time updates
static int index = 0,flag = 0, count = 0, event_count, menu_flag = 0;   // Counts total number of events,for gear count,
static unsigned int eeprom_address = 0X00;                              // Starting address for EEPROM storage
unsigned long int speed;                                                // Holds speed data from ADC reading
static int menu_cnt, i = 0, j = 0;                                      // Menu navigation variables
static int start_index = 0, current_index = 0;                          // For managing log view scroll positions
static unsigned char time_v[10][15];                                    // Log buffer for display
unsigned char clock_reg[3];                                             // Stores RTC time in BCD format
static unsigned char feild = 0;                                         // Field selector for time setting
unsigned char hrs, mins, secs;                                          // Time variables for hours, minutes, seconds
char *arr[] = {"ON", "GN", "GR", "G1", "G2", "G3", "G4", "G5", "C "};   // Event names (gear positions)
unsigned char *menu[] = {"VIEW LOG           ", "CLEAR LOG        ", "DOWNLOAD LOG          ", "SET TIME          "};   // Main menu options

// Function to initialize all peripherals and modules
static void init_config(void) {
    init_clcd();          // Initialize CLCD display
    init_matrix_keypad();  // Initialize matrix keypad
    init_adc();            // Initialize ADC for speed measurement
    init_i2c();            // Initialize I2C for RTC communication
    init_ds1307();         // Initialize DS1307 RTC module
    init_uart();           // Initialize UART for serial communication
}

//1.Function to check matrix keypad input and handle events
void check_matrix_keypad(void) {
    clcd_print(arr[count], LINE2(10));              // Displaying current event (gear) on CLCD    
    if (flag == 0 && key != ALL_RELEASED) {         // Check if in dashboard mode and key is pressed
        if (key == MK_SW1) {                        // Increment event count with SW1
            if (count == 8) { 
                count = 1;                             // Reset to initial event after collision
            } else {
                if (count < 7) {  
                    count++;                        // Next event(gears)
                    log_event();                    // Log the event in EEPROM
                }
            }
        }
        if (key == MK_SW2) {                        // Decrement event count with SW2
            if (count == 8) { 
                count = 1;
            } else {
                if (count > 1) {
                    count--;
                    log_event();                    // Log the event in EEPROM
                }
            }
        }
        if (key == MK_SW3) {                        // Collision event with SW3
            count = 8;
            log_event();                            // Log the event in EEPROM
        }

    }
}

//2.Function to read ADC, calculate, and display speed on CLCD
void display_speed(void) {
    unsigned long int adc_val;
    adc_val = read_adc(CHANNEL4);               // Read ADC value from speed sensor channel
    speed = (adc_val * 99) / 1023;              // Convert ADC value to speed

    clcd_putch((speed / 10) + '0', LINE2(14));  // Display tens digit of speed
    clcd_putch((speed % 10) + '0', LINE2(15));  // Display units digit of speed
}

//3. Time[RTC] part.Function to display time on CLCD from time array.
unsigned char time[9];
void display_time(void) {
    clcd_print(time, LINE2(0));
}
// Function to fetch time from DS1307 RTC module and format it
static void get_time(void) {
    clock_reg[0] = read_ds1307(HOUR_ADDR);  //read hours
    clock_reg[1] = read_ds1307(MIN_ADDR);   //read minutes
    clock_reg[2] = read_ds1307(SEC_ADDR);   //read seconds
    // Format BCD to ASCII for CLCD display
    if (clock_reg[0] & 0x40) {
        time[0] = '0' + ((clock_reg[0] >> 4) & 0x01);
        time[1] = '0' + (clock_reg[0] & 0x0F);
    } else {
        time[0] = '0' + ((clock_reg[0] >> 4) & 0x03);
        time[1] = '0' + (clock_reg[0] & 0x0F);
    }
    time[2] = ':';
    time[3] = '0' + ((clock_reg[1] >> 4) & 0x0F);
    time[4] = '0' + (clock_reg[1] & 0x0F);
    time[5] = ':';
    time[6] = '0' + ((clock_reg[2] >> 4) & 0x0F);
    time[7] = '0' + (clock_reg[2] & 0x0F);
    time[8] = '\0';
}

//4.Function to display main menu and navigate options
void display_menu(void) {
    // Display selector ('*') on the active menu option
    if (i == 0) {                           // 'i' indicates which line is currently selected (0 = first line, 1 = second line)
        clcd_putch('*', LINE1(0));          // Place '*' on the first line
        clcd_putch(' ', LINE2(0));          // Clear marker on the second line
    } else if (i == 1) {
        clcd_putch(' ', LINE1(0));
        clcd_putch('*', LINE2(0));
    }
    // Display the current pair of menu options based on the `menu_cnt` index.
    clcd_print(menu[menu_cnt], LINE1(2));           // Display first menu item
    clcd_print(menu[menu_cnt + 1], LINE2(2));       // Display second menu item
    
    // Menu navigation using MK_SW1 and MK_SW2 to scroll up/down
    if (key == MK_SW1) {
        if (i == 1) {
            i = 0;                              // If second line was selected, move to the first line
        } else if (menu_cnt > 0) {
            menu_cnt--;                         // Move to the previous menu options pair if not at the top
            i = 0;                              // Select the first line.
        }
    } else if (key == MK_SW2) {
        if (i == 0) {
            i = 1;                              // If first line was selected, move to the second line
        } else if (menu_cnt < 2) {
            menu_cnt++;                         // Move to the next menu options pair if not at the bottom
            i = 1;                              // Select the second line in the new pair
        }
    }
}

//5.this is log_event function to store every event in eeprom.
void log_event(void) {
    for (int i = 0; i < 8; i++) {               // Loop through the time array and store in EEPROM
        if (i == 2 || i == 5) {
            continue;                           // Skip positions 2 and 5 for ':'.
        } else {
            write_external_eeprom(eeprom_address++, time[i]);       // Write current time character to EEPROM
        }
    }
    // Stores two characters representing the event 
    write_external_eeprom(eeprom_address++, arr[count][0]);         // Write first event character
    write_external_eeprom(eeprom_address++, arr[count][1]);         // Write second event character
    // Log speed as two digits in EEPROM
    write_external_eeprom(eeprom_address++, (speed / 10) + '0');        //first , second digits of speed
    write_external_eeprom(eeprom_address++, (speed % 10) + '0');
    
     // Check if EEPROM address needs to be reset (log buffer reached capacity)
    if (eeprom_address >= 10 * 10) {
        eeprom_address = 0;                                         // reset back to start (circular buffer)
    }
    if (event_count < 10) {                                         // Update event count and start index for circular logging
        event_count++;                                              // increment the event count.
    } else {                                                        // If at max capacity of 10 events,
        start_index = (start_index + 1) % 10;                       // increment start_index for overwrite 
    }
}

//6.Function to view the events 
void view_log(void) {
    //    static unsigned char time_v[10][15];
    static unsigned int view_address = 0x00;                    // Stores the current EEPROM address to view logs
    
    static int view_flag = 0;                                   // Flag to indicate if logs have been loaded

    if (event_count == 0) {                                     // If no events are logged, display a message and return
        clcd_print("NO LOGS..          ", LINE1(0));
        clcd_print("  TO DISPLAY :(    ", LINE2(0));
        __delay_ms(1000);                                       // Delay before clearing the screen
        flag = 1;                                           // Set flag to get back to display_menu
        CLEAR_DISP_SCREEN;
    }

    else if (view_flag == 0) {
    // Loop through and read logs from EEPROM
    for (int i = 0; i < event_count; i++) {
        int entry_index = (start_index + i) % 10;           // Circular indexing for logs
        view_address = entry_index * 10;                    // Set address for each log entry
        // Read time data from EEPROM and store in time_v[] array
        time_v[i][0] = read_external_eeprom(view_address++);
        time_v[i][1] = read_external_eeprom(view_address++);
        time_v[i][2] = ':';
        time_v[i][3] = read_external_eeprom(view_address++);
        time_v[i][4] = read_external_eeprom(view_address++);
        time_v[i][5] = ':';
        time_v[i][6] = read_external_eeprom(view_address++);
        time_v[i][7] = read_external_eeprom(view_address++);
        time_v[i][8] = ' ';
        time_v[i][9] = read_external_eeprom(view_address++);
        time_v[i][10] = read_external_eeprom(view_address++);
        time_v[i][11] = ' ';
        time_v[i][12] = read_external_eeprom(view_address++);
        time_v[i][13] = read_external_eeprom(view_address++);
        time_v[i][14] = '\0'; // Null-terminate for CLCD display
    }
    view_flag = 1;                                      // Set flag to indicate logs have been loaded
    }
    
    // Handle navigation through the log entries using the switches
    if (key == MK_SW2 && index < event_count - 1) {
        index++;                                            // Move forward in the logs
    } else if (key == MK_SW1 && index > 0) {                // Move backward in the logs
        index--;
    }
    // Display the current log entry on the screen
    clcd_print(time_v[index], LINE2(2));
    clcd_putch(index + 48,LINE2(0));                    //to print index along with events
}

//7.function to clear the events
void clear_logs(void) {
    // Loop through all EEPROM addresses and set them to 0xFF (clear logs)
    for (int addr = 0; addr < eeprom_address; addr++) {
        write_external_eeprom(addr, 0xFF);                      // Write 0xFF to clear log data
    }
    eeprom_address = 0;  
    event_count = 0;     // Reset event count
    menu_cnt = 0;        // Reset menu counter
    flag = 1;            // Set flag to get to display menu
    i = 0;               // Reset index for '*' position in menu
    CLEAR_DISP_SCREEN;   // Clear the screen
}

//8.Function to download the logs and print in tera-term through UART.
void download_logs(void) {
    // If no logs are available, display message and return
    if (event_count == 0) {
        clcd_print("NO LOGS..          ", LINE1(0));
        clcd_print("  TO DOWNLOAD :(    ", LINE2(0));
        __delay_ms(1000);
        flag = 1;                           // Set flag to get onto display menu
        CLEAR_DISP_SCREEN;
    } else {                                // If logs are available, display downloading message
        clcd_print("Downloading         ", LINE1(0));
        clcd_print("Through UART        ", LINE2(0));
        __delay_ms(1000);
        flag = 1;                                        // Set flag to get onto display menu
        CLEAR_DISP_SCREEN;
    }
    puts("ID  TIME    EV  SP\n\r");                     // Print headers for the log data
    for (int i = 0; i < event_count; i++) {
        putch(i+48);                                    // Send index (converted to character)
        putch(' ');
        for (int j = 0; time_v[i][j] != '\0'; j++) {
            putch(time_v[i][j]);                            // Send each character of the time string(all data events))
        }
        puts("\n\r");                                   // Newline after each log entry and cursor position
    }
    CLEAR_DISP_SCREEN;
    menu_cnt = 0;
    i=0;
}

//9.Function to perform set time operation
unsigned long int delay = 0;
void set_time(void) {
    
    clcd_print("HH:MM:SS              ", LINE1(0));             // Display the format for time setting  
    if(feild == 0) {                                    //if field =0, it is at hours position
        if (delay++ < 500){
        clcd_putch((hrs / 10)+'0', LINE2(0));           //to print all field on CLCD
        clcd_putch((hrs % 10)+'0', LINE2(1));
        clcd_putch(':', LINE2(2));
        clcd_putch((mins / 10)+'0', LINE2(3));
        clcd_putch((mins % 10)+'0', LINE2(4));
        clcd_putch(':', LINE2(5));
        clcd_putch((secs / 10)+'0', LINE2(6));
        clcd_putch((secs % 10)+'0', LINE2(7));
        } 
        else if(delay++ < 1000) {                       //to make the blink effect for the selected field.
            clcd_putch(' ', LINE2(0));
            clcd_putch(' ', LINE2(1));
        }
        else        
            delay = 0;                          //reset delay to continue the blink
    }
        
    if(feild == 1) {                                        //if field =1, it is at minutes field
        if (delay++ < 500){
        clcd_putch((hrs / 10)+'0', LINE2(0));               //to print all field on CLCD
        clcd_putch((hrs % 10)+'0', LINE2(1));
        clcd_putch(':', LINE2(2));
        clcd_putch((mins / 10)+'0', LINE2(3));
        clcd_putch((mins % 10)+'0', LINE2(4));
        clcd_putch(':', LINE2(5));
        clcd_putch((secs / 10)+'0', LINE2(6));
        clcd_putch((secs % 10)+'0', LINE2(7));
        } 
        else if(delay++ < 1000) {                           //to make the blink effect for the selected field.
            clcd_putch(' ', LINE2(3));
            clcd_putch(' ', LINE2(4));
        }
        else        
            delay = 0;
    }
        clcd_putch(':', LINE2(5));
    if(feild == 2) {                                        //if field =2, it is at seconds field
        if (delay++ < 500){
        clcd_putch((hrs / 10)+'0', LINE2(0));               //to print all field on CLCD
        clcd_putch((hrs % 10)+'0', LINE2(1));
        clcd_putch(':', LINE2(2));
        clcd_putch((mins / 10)+'0', LINE2(3));
        clcd_putch((mins % 10)+'0', LINE2(4));
        clcd_putch(':', LINE2(5));
        clcd_putch((secs / 10)+'0', LINE2(6));
        clcd_putch((secs % 10)+'0', LINE2(7));
        } 
        else if(delay++ < 1000) {                            //to set the blink effect for the selected field.
            clcd_putch(' ', LINE2(6));
            clcd_putch(' ', LINE2(7));
        }
        else        
            delay = 0;
    }
        //based on switch press, field values get updated
        if (key == MK_SW2) {
            feild++;
            feild = (feild) % 3;
            
        }
        //To increment the time value of selected field and modify the time.
        if (key == MK_SW1) {
            if (feild == 0) {
                hrs = (hrs + 1) % 24;
            } else if (feild == 1) {
                mins = (mins + 1) % 60;
            } else if (feild == 2) {
                secs = (secs + 1) % 60;
            }
        }   
        //SW11 will make the modified time get saved to dashboard. 
        if (key == MK_SW11) {
        s_flag = 0;
        write_ds1307(HOUR_ADDR, ((hrs / 10) << 4) | (hrs % 10));        //save hour
        write_ds1307(MIN_ADDR, ((mins / 10) << 4) | (mins % 10));       //save minutes
        write_ds1307(SEC_ADDR, ((secs / 10) << 4) | (secs % 10));       //save seconds
        get_time();                                         // Update the time from the RTC
        flag = 0;
    }
    menu_cnt = 0;                   //to get onto the line of the menu options
    i=0;
}


void main(void) {
    init_config();                  // Initialize configuration settings
    flag = 0;                       // Start in Dashboard mode (flag=0 indicates the dashboard view)

    while (1) {
        key = read_switches(STATE_CHANGE);

        // Dashboard Mode: Default mode when the system starts
        if (flag == 0) {
            clcd_print("  TIME    EV  SP", LINE1(0));           // Display the header on the dashboard
            check_matrix_keypad();                              //function call to display the gears
            display_speed();                                    // function call to Display the current speed
            display_time();                                     //function call
            get_time();

            if (key == MK_SW11) {                       // Transition to the Main Menu if SW11 (a specific button) is pressed
                CLEAR_DISP_SCREEN;
                flag = 1;                               // Move to Main Menu
            }
        }           
        // Main Menu Mode: When the system is in the main menu to select options
        else if (flag == 1) {
            display_menu();                         // Show menu options (View Log, Clear Log, etc.)

            if (key == MK_SW11) {                   // SW11 selects the highlighted option
                if (menu_cnt == 0 && i == 0) {      // View Log option(based on menu count and i))
                    CLEAR_DISP_SCREEN;
                    clcd_print("#   VIEW LOG           ", LINE1(0));
                    flag = 2;                       // Set flag to 2 to enter the "View Log" mode
                    menu_flag = 1;                  // Set menu_flag to 1 to activate view log display                  
                } 
                // If "Clear Log" is selected (based on menu_cnt and i values)
                else if ((menu_cnt == 0 && i == 1) || (menu_cnt == 1 && i == 0)) {    
                    CLEAR_DISP_SCREEN;
                    clcd_print("Clearing Logs...", LINE1(0));
                    clcd_print("Just wait a sec..", LINE2(0));
                    __delay_ms(1000);
                    flag = 3;                           // Set flag to 3 to enter the "Clear Log" mode
                    menu_flag = 2;                      // Set menu_flag to 2 to activate clear log display
                } 
                // If "Download Log" is selected
                else if ((menu_cnt == 1 && i == 1) || (menu_cnt == 2 && i == 0)) { // download Log option
                    CLEAR_DISP_SCREEN;
                    flag = 4;                           // Set flag to 4 to enter the "Download Log" mode
                    menu_flag = 3;                      // Set menu_flag to 3 to activate download log display                 
                } 
                // If "Set Time" is selected
                else if (menu_cnt == 2 && i == 1) { 
                    CLEAR_DISP_SCREEN;
                    //clcd_print("#   SET TIME           ", LINE1(0));
                    flag = 5;                           // Set flag to 5 to enter the "Set Time" mode
                    menu_flag = 4;                      // Set menu_flag to 4 to activate set time display
                }
            }
            // If SW12 is pressed, return to Dashboard mode
            if (key == MK_SW12) { 
                CLEAR_DISP_SCREEN;
                flag = 0;                           // Return to Dashboard mode
                menu_cnt = 0;
                menu_flag = 0;
            }
        }// View Log Mode
        else if (flag == 2 && menu_flag == 1) {
            view_log(); // function call
            
            if (key == MK_SW12) {                       // SW12 exits View Log back to Main Menu
                index =0 ;
                CLEAR_DISP_SCREEN;
                flag = 1;                               // Return to Main Menu
                menu_flag = 0;                          // Reset view log display flag
            }
        }// Clear Log Mode
        else if (flag == 3 && menu_flag == 2) {
            clear_logs();   //function call                            
            
            if (key == MK_SW12) {                       // SW12 exits Clear Log back to Main Menu
                CLEAR_DISP_SCREEN;
                flag = 1;                               // Return to Main Menu
                menu_flag = 0;                          // Reset clear log display flag
            }
        }//Download log mode
        else if (flag == 4 && menu_flag == 3) {
            download_logs();    // function call
            flag = 1;                                   // Return to Main Menu
            menu_flag = 0;                              // Reset download log display flag
        }//set time mode
        else if (flag == 5 && menu_flag == 4) {
            if ( s_flag == 0)                           //to read the dashboard displayed time at once.
            {
            hrs = ((time[0] - 48 )*10) + (time[1] - 48);
            mins = ((time[3] - 48 )*10) + (time[4] - 48);
            secs = ((time[6] - 48 )*10) + (time[7] - 48);
            s_flag = 1;
            }
            set_time();                 // function call
            if (key == MK_SW12) {                           // SW12 exits Clear Log back to Main Menu
                CLEAR_DISP_SCREEN;
                flag = 1;                                   // Return to Main Menu
                menu_flag = 0; 
                s_flag = 0;
            }
        }
    }
}

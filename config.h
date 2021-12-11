/* Configuration file
 *  
 */

#ifndef CONFIGH
#define CONFIGH


#define CONSOLE Serial
#define BMS Serial1

#define ADDR_SIZE 1 //0 is 8 bit register addresses (TI recommended), 1 is 16 bit (used by BMW)
#define MAX_MODULES 16 //maximum of 16 modules in a pack

#endif //CONFIGH

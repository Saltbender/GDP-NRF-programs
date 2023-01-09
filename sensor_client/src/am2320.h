#ifndef _AM2320_H__
#define _AM2320_H__

/** @brief Function to initialise an i2c device. 
 * 
*/

int initI2C (struct device* dev);

/** @brief Function to retrieve data from the AM2320 sensor, requires an initialiased i2c device as argument 
 * 
*/

int getSensorValues(const struct device* dev, uint16_t* humidity,uint16_t* temperature);

#endif 
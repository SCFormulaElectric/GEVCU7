/*
 * PotThrottle.cpp
 *
 Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */
#include "PotThrottle.h"

/*
 * Constructor
 */
PotThrottle::PotThrottle() : Throttle() {    
    commonName = "Potentiometer (analog) accelerator";
    shortName = "PotAccel";
}

void PotThrottle::earlyInit()
{
    prefsHandler = new PrefHandler(POTACCELPEDAL);
}

/*
 * Setup the device.
 */
void PotThrottle::setup() {
    crashHandler.addBreadcrumb(ENCODE_BREAD("PTTHR") + 0);
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: PotThrottle (id: %X, %X)", POTACCELPEDAL, this);

    loadConfiguration();

    PotThrottleConfiguration *config = (PotThrottleConfiguration *) getConfiguration();

    Throttle::setup(); //call base class

    ConfigEntry entry;
    entry = {"TPOT", "Number of pots to use (1 or 2)", &config->numberPotMeters, CFG_ENTRY_VAR_TYPE::BYTE, 1, 2, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TTYPE", "Set throttle subtype (1=std linear, 2=inverse)", &config->throttleSubType, CFG_ENTRY_VAR_TYPE::BYTE, 1, 2, 0, DEV_PTR(&PotThrottle::describeThrottleType)};
    cfgEntries.push_back(entry);
    entry = {"T1ADC", "Set throttle 1 ADC pin", &config->AdcPin1, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"T1MN", "Set throttle 1 min value", &config->minimumLevel1, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"T1MX", "Set throttle 1 max value", &config->maximumLevel1, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"T2ADC", "Set throttle 2 ADC pin", &config->AdcPin2, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"T2MN", "Set throttle 2 min value", &config->minimumLevel2, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"T2MX", "Set throttle 2 max value", &config->maximumLevel2, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    tickHandler.attach(this, CFG_TICK_INTERVAL_POT_THROTTLE);
}

/*
 * Process a timer event.
 */
void PotThrottle::handleTick() {
    crashHandler.addBreadcrumb(ENCODE_BREAD("PTTHR") + 1);
    Throttle::handleTick(); // Call parent which controls the workflow
}

/*
 * Retrieve raw input signals from the throttle hardware.
 */
RawSignalData *PotThrottle::acquireRawSignal() {
    PotThrottleConfiguration *config = (PotThrottleConfiguration *) getConfiguration();
    rawSignal.input1 = systemIO.getAnalogIn(config->AdcPin1);
    rawSignal.input2 = systemIO.getAnalogIn(config->AdcPin2);
    return &rawSignal;
}

/*
 * Perform sanity check on the ADC input values. The values are normalized (without constraining them)
 * and the checks are performed on a 0-1000 scale with a percentage tolerance
 */
bool PotThrottle::validateSignal(RawSignalData *rawSignal) {
    PotThrottleConfiguration *config = (PotThrottleConfiguration *) getConfiguration();
    int32_t calcThrottle1, calcThrottle2;
    //maps throttle sensor one from a range of 0 to 1000 where each # is a tenth of a percent
    calcThrottle1 = normalizeInput(rawSignal->input1, config->minimumLevel1, config->maximumLevel1);
    if (config->numberPotMeters == 1 && config->throttleSubType == 2) { // inverted
        calcThrottle1 = 1000 - calcThrottle1;
    }
    //if the throttle reading is above 100.0% + the tolerance (15.0%, set in Throttle.h), then throw an error
    if (calcThrottle1 > (1000 + CFG_THROTTLE_TOLERANCE))
    {
        if (status == OK)
            Logger::error(POTACCELPEDAL, "ERR_HIGH_T1: throttle 1 value out of range: %i", calcThrottle1);
        status = ERR_HIGH_T1;
        faultHandler.raiseFault(POTACCELPEDAL, FAULT_THROTTLE_HIGH_A, true);
        return false;
    }
    //if the throttle reading is above 100.0% but below the tolerance, then set reading to 100.0%
    else
    {
        if (calcThrottle1 > 1000) calcThrottle1 = 1000;
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_HIGH_A);
    }
    //if the throttle reading is below 0.0% - tolerance (15.0%), then throw an error
    if (calcThrottle1 < (0 - CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(POTACCELPEDAL, "ERR_LOW_T1: throttle 1 value out of range: %i ", calcThrottle1);
        status = ERR_LOW_T1;
        faultHandler.raiseFault(POTACCELPEDAL, FAULT_THROTTLE_LOW_A, true);
        return false;
    }
    //if the throttle reading is below 0.0%, but above the tolerance, set the reading to 0.0%
    else
    {
        if (calcThrottle1 < 0) calcThrottle1 = 0;
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_LOW_A);
    }

    calcThrottle2 = normalizeInput(rawSignal->input2, config->minimumLevel2, config->maximumLevel2);
    //if the throttle reading is above 100.0% + the tolerance (15.0%), then throw an error
    if (calcThrottle2 > (1000 + CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(POTACCELPEDAL, "ERR_HIGH_T2: throttle 2 value out of range: %i", calcThrottle2);
        status = ERR_HIGH_T2;
        faultHandler.raiseFault(POTACCELPEDAL, FAULT_THROTTLE_HIGH_B, true);
        return false;
    }
    //if the throttle reading is above 100.0% but below the tolerance, then set reading to 100.0%
    else
    {
        if (calcThrottle2 > 1000) calcThrottle2 = 1000;
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_HIGH_B);
    }
    //if the throttle reading is below 0.0% - tolerance (15.0%), then throw an error
    if (calcThrottle2 < (0 - CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(POTACCELPEDAL, "ERR_LOW_T2: throttle 2 value out of range: %i", calcThrottle2);
        status = ERR_LOW_T2;
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_LOW_B);
        return false;
    }
    //if the throttle reading is below 0.0%, but above the tolerance, set the reading to 0.0%
    else
    {
        if (calcThrottle2 < 0) calcThrottle2 = 0;
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_LOW_B);
    }
    //if the throttle readings difference is greater than the ThrottleMaxErrValue (10.0% set by guidelines), then throw an error
    //throttle 1 sensor is too big
    if ((calcThrottle1 - ThrottleMaxErrValue) > calcThrottle2) { 
        if (status == OK)
            Logger::error(POTACCELPEDAL, "throttle 1 too high (%i) compared to 2 (%i)", calcThrottle1, calcThrottle2);
        status = ERR_MISMATCH;
        faultHandler.raiseFault(POTACCELPEDAL, FAULT_THROTTLE_MISMATCH_AB, true);
        return false;
    }
    //throttle 2 sensor is too big
    else if ((calcThrottle2 - ThrottleMaxErrValue) > calcThrottle1) { //then throttle2 is too large compared to 1
        if (status == OK)
            Logger::error(POTACCELPEDAL, "throttle 2 too high (%i) compared to 1 (%i)", calcThrottle2, calcThrottle1);
        status = ERR_MISMATCH;
        faultHandler.raiseFault(POTACCELPEDAL, FAULT_THROTTLE_MISMATCH_AB, true);
        return false;
    }
    //throttle readings are good now, so cancel any faults that are happening
    else
    {
        faultHandler.cancelOngoingFault(POTACCELPEDAL, FAULT_THROTTLE_MISMATCH_AB);
    }
        

    // all checks passed -> throttle is ok
    if (status != OK)
        if (status != ERR_MISC)
        { 
            Logger::info(POTACCELPEDAL, (char *)Constants::normalOperation);
        }
    status = OK;
    return true;
}

/*
 * Convert the raw ADC values to a range from 0 to 1000 (per mille) according
 * to the specified range and the type of potentiometer.
 */
int16_t PotThrottle::calculatePedalPosition(RawSignalData *rawSignal) {
    PotThrottleConfiguration *config = (PotThrottleConfiguration *) getConfiguration();
    uint16_t calcThrottle1, calcThrottle2;

    calcThrottle1 = normalizeInput(rawSignal->input1, config->minimumLevel1, config->maximumLevel1);
    calcThrottle2 = normalizeInput(rawSignal->input2, config->minimumLevel2, config->maximumLevel2);
    return ((calcThrottle1 + calcThrottle2) / 2); //return average percentage between two readings

}

/*
 * Return the device ID
 */
DeviceId PotThrottle::getId() {
    return (POTACCELPEDAL);
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void PotThrottle::loadConfiguration() {
    PotThrottleConfiguration *config = (PotThrottleConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new PotThrottleConfiguration();
        Logger::debug("loading configuration in throttle");
        setConfiguration(config);
    }

    Throttle::loadConfiguration(); // call parent

 
    Logger::debug(POTACCELPEDAL, Constants::validChecksum);
    //Configuration for our throttle pedal
    //The input for max and min are in weird units where 1 volt = 818 mystery unit
    //Example: 5 volts = 4090 mystery unit, so if the max reading is 5 volts, input 4090 to maximumLevel1/maximumLevel2

    //these values should be changed to match real throttle pedal, not the manual numbers
    prefsHandler->read("ThrottleMin1", (uint16_t *)&config->minimumLevel1, 614);     //should be voltage sensor 1   
    prefsHandler->read("ThrottleMax1", (uint16_t *)&config->maximumLevel1, 3170);
    prefsHandler->read("ThrottleMin2", (uint16_t *)&config->minimumLevel2, 303);      //should be voltage sensor 2, the one with half the voltage reading  
    prefsHandler->read("ThrottleMax2", (uint16_t *)&config->maximumLevel2, 1583);
    //We have 2 throttle sensors so input 2
    prefsHandler->read("NumThrottles", &config->numberPotMeters, 2);
    //Our sensors are positive linear which is throttleSubType 1
    prefsHandler->read("ThrottleType", &config->throttleSubType, 1);
    //Set Analog inputs to their respective pins
    //For some reason AnalogIn0 is pin 0 and AnalogIn1 is pin 4, found through testing
    //voltage sensor 1 should be in AnalogIn0
    //voltage sensor 2 should be in AnalogIn1
    prefsHandler->read("ADC1", &config->AdcPin1, 0);
    prefsHandler->read("ADC2", &config->AdcPin2, 4);
    Logger::debug(POTACCELPEDAL, "# of pots: %d       subtype: %d", config->numberPotMeters, config->throttleSubType);
    Logger::debug(POTACCELPEDAL, "T1 MIN: %i MAX: %i      T2 MIN: %i MAX: %i", config->minimumLevel1, config->maximumLevel1, config->minimumLevel2,
                  config->maximumLevel2);
}
//creation of a global variable here causes the driver to automatically register itself without external help
PotThrottle potThrottle;



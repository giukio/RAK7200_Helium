/*!
 *  @file rak7200.cpp
 *
 *  BSD 3-Clause License
 *  Copyright (c) 2021, Giulio Berti
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer.
 *  
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  
 */

#include <Arduino.h>
#include <deviceBase.h>
#include <devices\rak7200.h>
#include <SPI.h>
#include <lora.h>


Rak7200::Rak7200(){
    _GNSS = new HardwareSerial(S7xG_CXD5603_UART_RX, S7xG_CXD5603_UART_TX);
    if (_GNSS == NULL)
    {
        Serial.println("ERROR: Couldn't Configure GNSS Hardware Serial");
        while(1);
    }

    _lis = new Adafruit_LIS3DH(new TwoWire(S7xx_I2C_SDA, S7xx_I2C_SCL));
}

void Rak7200::setConsole(){
  #ifndef ESP8266
    while (!Serial) yield();     // will pause Zero, Leonardo, etc until serial console opens
  #endif

  // Configure AcSiP S7xx Serial1 to Arduino Serial
  Serial.setTx(S7xx_CONSOLE_TX);
  Serial.setRx(S7xx_CONSOLE_RX);
  Serial.begin(115200);
}

bool Rak7200::GNSS_probe() {
    unsigned long startTime = millis();
    char c1, c2;
    c1 = c2 = 0;

    Rak7200::_GNSS->flush();
    while (millis() - startTime < 3000) {
        if (Rak7200::_GNSS->available() > 0) {
            c1 = Rak7200::_GNSS->read();
            if ((c1 == '$') && (c2 == 0)) {
                c2 = c1;
                continue;
            }
            if ((c2 == '$') && (c1 == 'G')) {
                // got $G leave the function with GNSS port opened
                return true;
            }
            else {
                c2 = 0;
            }
        }
        delay(1);
    }
    return false;
}

void Rak7200::setGps(){
    // power on GNSS
    Serial.println("Powering On GNSS");
    pinMode(RAK7200_S76G_CXD5603_POWER_ENABLE, OUTPUT);
    digitalWrite(RAK7200_S76G_CXD5603_POWER_ENABLE, HIGH);
    delay(1200); // Delay 315µs to 800µs ramp up time

    _GNSS->begin(S7xG_CXD5603_BAUD_RATE);

    /* drive GNSS RST pin low */
    pinMode(S7xG_CXD5603_RESET, OUTPUT);
    digitalWrite(S7xG_CXD5603_RESET, LOW);

    /* activate 1.8V<->3.3V level shifters */
    pinMode(S7xG_CXD5603_LEVEL_SHIFTER, OUTPUT);
    digitalWrite(S7xG_CXD5603_LEVEL_SHIFTER, HIGH);

    /* keep RST low to ensure proper IC reset */
    delay(250);

    /* release */
    digitalWrite(S7xG_CXD5603_RESET, HIGH);

    /* give Sony GNSS few ms to warm up */
    delay(125);

    /* configure GNSS */
    // _GNSS.write("@GCD\r\n"); // Cold start
    // delay(500);
    //_GNSS.write("@GSW\r\n"); // Warm start
    //delay(500);
    Rak7200::_GNSS->write("@GSP\r\n"); // Hot start for position accuracy
    delay(500);
    //_GNSS.write("@GPPS 0x1\r\n"); // Enable PPS
    //delay(125);
    /*
     * @GNS Select the satellite systems to be used for positioning
     * bit 0 : GPS          0x01
     * bit 1 : GLONASS      0x02
     * bit 2 : SBAS         0x04
     * bit 3 : QZSS L1-CA   0x08
     *
     */
    Rak7200::_GNSS->write("@GNS 0x7\r\n"); // Configure GPS, GLONASS, SBAS
    //_GNSS.write("@GNS 0x5\r\n"); // Configure GPS, SBAS
    //_GNSS.write("@GNS 0x1\r\n"); // Configure GPS
    //_GNSS.write("@GNS 0x2\r\n"); // Configure GLONASS
    delay(125);
    /*
     *
     * @BSSL Select NMEA sentences to output
     * bit0 : GGA 0x01
     * bit1 : GLL 0x02
     * bit2 : GSA 0x04
     * bit3 : GSV 0x08
     * bit4 : GNS 0x10
     * bit5 : RMC 0x20
     * bit6 : VTG 0x40
     * bit7 : ZDA 0x80
     *
     */
    //_GNSS.write("@BSSL 0xFF\r\n"); // All NMEA sentences
    //_GNSS.write("@BSSL 0xFE\r\n"); // All NMEA sentences but GGA
    //_GNSS.write("@BSSL 0xB3\r\n"); // GGA, GLL, GNS, RMC, ZDA
    //_GNSS.write("@BSSL 0xA1\r\n"); // GGA, RMC, ZDA
    Rak7200::_GNSS->write("@BSSL 0x21\r\n"); // GGA and RMC
    delay(125);

    Serial.print("GNSS   - ");
    Serial.println(Rak7200::GNSS_probe() ? "PASS" : "FAIL");  

    _GNSS->begin(115200);
    while (!_GNSS);
    Serial.println("GNSS UART Initialized");
}

gps_fix Rak7200::getGpsFix(){
    while (Rak7200::gps.available(*_GNSS)) {
        Rak7200::fix = Rak7200::gps.read();
    }
    return Rak7200::fix;
}

void Rak7200::setLora(){
    u1_t PROGMEM buf[16];
    nvRead(DEVEUI, NV_DEVEUI, 8);
    Serial.print("NV_DEVEUI: ");for (uint16_t i = 0; i < 8; i++){printHex2(DEVEUI[i]);}Serial.println();   

    nvRead(APPEUI, NV_APPEUI, 8);
    Serial.print("NV_APPEUI: ");for (uint16_t i = 0; i < 8; i++){printHex2(APPEUI[i]);}Serial.println();   

    nvRead(APPKEY, NV_APPKEY, 16);
    Serial.print("NV_APPKEY: ");for (uint16_t i = 0; i < 16; i++){printHex2(APPKEY[i]);}Serial.println();   

    SPI.setMISO(S7xx_SX127x_MISO);
    SPI.setMOSI(S7xx_SX127x_MOSI);
    SPI.setSCLK(S7xx_SX127x_SCK);
    SPI.setSSEL(S7xx_SX127x_NSS);

    SPI.begin();

    digitalWrite(S7xx_SX127x_NSS, HIGH);
    pinMode(S7xx_SX127x_NSS, OUTPUT);

    digitalWrite(S7xx_SX127x_NRESET, HIGH);
    pinMode(S7xx_SX127x_NRESET, OUTPUT);

    // manually reset radio
    digitalWrite(S7xx_SX127x_NRESET, LOW);
    delay(5);
    digitalWrite(S7xx_SX127x_NRESET, HIGH);
    delay(5);

    digitalWrite(S7xx_SX127x_NSS, LOW);

    SPI.transfer(SX1276_RegVersion & 0x7F);
    _has_SX1276 = (SPI.transfer(0x00) == 0x12 ? true : false);

    digitalWrite(S7xx_SX127x_NSS, HIGH);

    SPI.end();
    pinMode(S7xx_SX127x_NSS, INPUT);
    pinMode(S7xx_SX127x_NRESET, INPUT);

    Serial.println("Built-in components:");

    Serial.print("RADIO  - ");
    Serial.println(_has_SX1276 ? "PASS" : "FAIL");
}

// LMIC library expects pinmap as global constant
const lmic_pinmap lmic_pins = {
        .nss = S7xx_SX127x_NSS,
        .rxtx = S7xx_SX127x_ANTENNA_SWITCH_RXTX,
        .rst = S7xx_SX127x_NRESET,
        .dio = {S7xx_SX127x_DIO0, S7xx_SX127x_DIO1, S7xx_SX127x_DIO2},
        .rxtx_rx_active = 1,
        .rssi_cal = 10,
        .spi_freq = 1000000
};

void Rak7200::setSensors(){
    this->setLis3dh();
}

void Rak7200::setLis3dh(){
    if (this->_lis->begin(0x19) == false){
        Serial.println("Couldnt Start LIS3DH Accelerometer");
        while (1)
        {
            yield();
        }
    }
    Serial.println("LIS3DH found!");

    this->_lis->enableDRDY(false,1);    // Leave INT1 PIN free
    
    this->_lis->setRange(LIS3DH_RANGE_2_G);   // 2, 4, 8 or 16 G!
    
    Serial.print("Range = "); Serial.print(2 << this->_lis->getRange());  
    Serial.println("G");

    // this->_lis->setDataRate(LIS3DH_DATARATE_50_HZ);
    Serial.print("Data rate set to: ");
    switch (this->_lis->getDataRate()) {
        case LIS3DH_DATARATE_1_HZ: Serial.println("1 Hz"); break;
        case LIS3DH_DATARATE_10_HZ: Serial.println("10 Hz"); break;
        case LIS3DH_DATARATE_25_HZ: Serial.println("25 Hz"); break;
        case LIS3DH_DATARATE_50_HZ: Serial.println("50 Hz"); break;
        case LIS3DH_DATARATE_100_HZ: Serial.println("100 Hz"); break;
        case LIS3DH_DATARATE_200_HZ: Serial.println("200 Hz"); break;
        case LIS3DH_DATARATE_400_HZ: Serial.println("400 Hz"); break;

        case LIS3DH_DATARATE_POWERDOWN: Serial.println("Powered Down"); break;
        case LIS3DH_DATARATE_LOWPOWER_5KHZ: Serial.println("5 Khz Low Power"); break;
        case LIS3DH_DATARATE_LOWPOWER_1K6HZ: Serial.println("16 Khz Low Power"); break;
    }

    // 0 = turn off click detection & interrupt
    // 1 = single click only interrupt output
    // 2 = double click only interrupt output, detect single click
    // Adjust threshhold, higher numbers are less sensitive
    this->_lis->setClick(2, 40);
    pinMode(RAK7200_S76G_LIS3DH_INT1, INPUT);
    attachInterrupt(digitalPinToInterrupt(RAK7200_S76G_LIS3DH_INT1), Rak7200::Lis3dhInt1_ISR, RISING);
}

int8_t Rak7200::getTemperature(){
    return this->_lis->readTemperature(13);
}

std::vector<float> Rak7200::getAcceleration(){
    this->_lis->read();
    return std::vector<float>{ this->_lis->x_g, this->_lis->y_g, this->_lis->z_g };
}

void Rak7200::Lis3dhInt1_ISR(){
    dev.deviceMoving();
}

void Rak7200::deviceMoving(){
        if (this->isMoving() == false)
        {
            this->Lis3dhInt1Flag = true;
            // Device just started moving
            Serial.print(_lastMotionMillis); Serial.println(": Motion detected.");
        }
        
        this->_lastMotionMillis = millis();
}

bool Rak7200::isMoving(){
    bool gpsValid = (this->fix.satellites >= 3 ) && (this->fix.speed_kph() > this->_motionGpsSpeedThreshold);
    return ((millis() - this->_lastMotionMillis) < this->_motionWindowMs) || gpsValid;
}

bool Rak7200::isMotionJustStarted(){
    if (this->Lis3dhInt1Flag)
    {
        this->Lis3dhInt1Flag = false;
        return true;
    }
    else
    {
        return false;
    }
}

void Rak7200::DumpEeprom(){
    for (uint32_t i = 0; i < 0x1800; i++)
    {
        Serial.printf("%02X",this->ee.readEEPROM8bit(i));
    }

    // uint32_t addr = 0x0B00;
    // Serial.printf("%04X - %08X\r\n", addr, this->ee.readEEPROM32bit(addr));
    // if (this->ee.writeEEPROM(addr+1, (uint8_t)0x11) == HAL_StatusTypeDef::HAL_ERROR)
    // {
    //     Serial.printf("EEprom Error, coundn't write to address %04X", addr+1);
    //     Serial.flush();
    //     return;
    // }
    // Serial.printf("%04X - %08X\r\n", addr, this->ee.readEEPROM32bit(addr+1));
}

int8_t Rak7200::nvWrite(uint32_t address, uint8_t data ){
    return (this->ee.writeEEPROM(address, data) == HAL_StatusTypeDef::HAL_OK) ? 0 : -1;
}

int8_t Rak7200::nvWrite(uint32_t address, uint16_t data){
    return (this->ee.writeEEPROM(address, data) == HAL_StatusTypeDef::HAL_OK) ? 0 : -1;
}

int8_t Rak7200::nvWrite(uint32_t address, uint32_t data){
    return (this->ee.writeEEPROM(address, data) == HAL_StatusTypeDef::HAL_OK) ? 0 : -1;
}

int8_t Rak7200::nvWrite(uint32_t address, uint64_t data){
    union u64
    {
        uint64_t d64;
        uint32_t d32[2];
    }split;
    split.d64 = data;

    if (address % 8) return -2; // Address not aligned
    if(this->ee.writeEEPROM(address, split.d32[0]) != HAL_StatusTypeDef::HAL_OK){
        return -1;
    }
    if(this->ee.writeEEPROM(address+4, split.d32[1]) != HAL_StatusTypeDef::HAL_OK){
        return -1;
    }

    return 0;
}

int8_t Rak7200::nvWrite(uint32_t address, uint8_t* data, uint16_t num){
    for (int16_t i = 0; i < num; i++)
    {
        if (this->ee.writeEEPROM(address+i, data[i]) != HAL_StatusTypeDef::HAL_OK) {
            return 1;
        }
    }
    return 0;
}

uint8_t Rak7200::nvRead8bit(uint32_t address  ){
    return this->ee.readEEPROM8bit(address);
}

uint16_t Rak7200::nvRead16bit(uint32_t address){
    return this->ee.readEEPROM16bit(address);
}

uint32_t Rak7200::nvRead32bit(uint32_t address){
    return this->ee.readEEPROM32bit(address);
}

uint64_t Rak7200::nvRead64bit(uint32_t address){
    union u64
    {
        uint64_t d64;
        uint32_t d32[2];
    }split;

    if (address % 8) return 0; // Address not aligned
    split.d32[0] = this->ee.readEEPROM32bit(address);
    split.d32[1] = this->ee.readEEPROM32bit(address+4);
    return split.d64;
}

void Rak7200::nvRead(uint8_t* dest, uint32_t address, uint16_t num){
    for (uint16_t i = 0; i < num; i++)
    {
        dest[i] = this->ee.readEEPROM8bit(address+i);
    }
}


/**
 * writeEEPROM allows to write a byte(uint8_t) to the internal eeprom
 * @param   address  starts at 0, the max size depends on the uc type
 * @param   data     byte (uint8_t)
 * @return  status   internal HAL_Status
 */
HAL_StatusTypeDef Eeprom::writeEEPROM(uint32_t address, uint8_t data) {
    HAL_StatusTypeDef  status;
    if (address >= EEPROM_SIZE)
    {
        return HAL_StatusTypeDef::HAL_ERROR;
    }
    
    address = address + EEPROM_BASE_ADDRESS;
    HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing
    status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_BYTE, address, data);
    HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
    return status;
}

/**
 * writeEEPROMByte allows to write a half word(uint16_t) to the internal eeprom
 * @param   address  starts at 0, the max size depends on the uc type
 * @param   data     byte (uint16_t)
 * @return  status   internal HAL_Status
 */
HAL_StatusTypeDef Eeprom::writeEEPROM(uint32_t address, uint16_t data) {
    HAL_StatusTypeDef  status;
    if ((address >= EEPROM_SIZE)||(address % 2))
    {
        return HAL_StatusTypeDef::HAL_ERROR;
    }
    
    address = address + EEPROM_BASE_ADDRESS;
    HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing
    status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_HALFWORD, address, data);
    HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
    return status;
}

/**
 * writeEEPROMByte allows to write a word(uint32_t) to the internal eeprom
 * @param   address  starts at 0, the max size depends on the uc type
 * @param   data     byte (uint32_t)
 * @return  status   internal HAL_Status
 */
HAL_StatusTypeDef Eeprom::writeEEPROM(uint32_t address, uint32_t data) {
    HAL_StatusTypeDef  status;
    if ((address >= EEPROM_SIZE)||(address % 4))
    {
        return HAL_StatusTypeDef::HAL_ERROR;
    }
    
    address = address + EEPROM_BASE_ADDRESS;
    HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing
    status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_WORD, address, data);
    HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
    return status;
}

/**
 * readEEPROMByte reads a byte from an internal eeprom
 * @param   address  of the eeprom byte
 * @return  data     as a byte (uint8_t)
 */
uint8_t Eeprom::readEEPROM8bit(uint32_t address) {
    uint8_t data = 0;
    address = address + EEPROM_BASE_ADDRESS;
    data = *(__IO uint8_t*)address;
    return data;
}

uint16_t Eeprom::readEEPROM16bit(uint32_t address) {
    uint16_t data = 0;
    if ((address >= EEPROM_SIZE)||(address % 2))
    {
        return 0;
    }
    address = address + EEPROM_BASE_ADDRESS;
    data = *(__IO uint16_t*)address;
    return data;
}

uint32_t Eeprom::readEEPROM32bit(uint32_t address) {
    uint32_t data = 0;
    if ((address >= EEPROM_SIZE)||(address % 4))
    {
        return 0;
    }
    address = address + EEPROM_BASE_ADDRESS;
    data = *(__IO uint32_t*)address;
    return data;
}


Eeprom::Eeprom(/* args */)
{
}

Eeprom::~Eeprom()
{
}

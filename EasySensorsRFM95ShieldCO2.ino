/*
  Copyright ® 2018 November devMobile Software, All Rights Reserved

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  You can do what you want with this code, acknowledgment would be nice.

  http://www.devmobile.co.nz

*/
#include <stdlib.h>
#include <LoRa.h>
#include <sha204_library.h>
#include "SCD30.h"

//#define DEBUG
//#define DEBUG_TELEMETRY
//#define DEBUG_LORA

// LoRa field gateway configuration (these settings must match your field gateway)
const byte DeviceAddressMaximumLength = 15 ;
const char FieldGatewayAddress[] = {"LoRaIoT1"};
const float FieldGatewayFrequency =  915000000.0;
const byte FieldGatewaySyncWord = 0x12 ;

// Payload configuration
const int ChipSelectPin = 10;
const int ResetPin = 9;
const int InterruptPin = 2;

// LoRa radio payload configuration
const byte SensorIdValueSeperator = ' ' ;
const byte SensorReadingSeperator = ',' ;
const unsigned long SensorUploadDelay = 300000;

// ATSHA204 secure authentication, validation with crypto and hashing (currently only using for unique serial number)
const byte Atsha204Port = A3;
atsha204Class sha204(Atsha204Port);
const byte DeviceSerialNumberLength = 9 ;
byte deviceSerialNumber[DeviceSerialNumberLength] = {""};

const byte PayloadSizeMaximum = 64 ;
byte payload[PayloadSizeMaximum];
byte payloadLength = 0 ;


void setup()
{
  Serial.begin(9600);

#ifdef DEBUG
  while (!Serial);
#endif
 
  Serial.println("Setup called");

  Serial.print("Field gateway:");
  Serial.print(FieldGatewayAddress ) ;
  Serial.print(" Frequency:");
  Serial.print( FieldGatewayFrequency,0 ) ;
  Serial.print("MHz SyncWord:");
  Serial.print( FieldGatewaySyncWord ) ;
  Serial.println();
  
   // Retrieve the serial number then display it nicely
  if(sha204.getSerialNumber(deviceSerialNumber))
  {
    Serial.println("sha204.getSerialNumber failed");
    while (true); // Drop into endless loop requiring restart
  }

  Serial.print("SNo:");
  DisplayHex( deviceSerialNumber, DeviceSerialNumberLength);
  Serial.println();

  Serial.println("LoRa setup start");

  // override the default chip select and reset pins
  LoRa.setPins(ChipSelectPin, ResetPin, InterruptPin);
  if (!LoRa.begin(FieldGatewayFrequency))
  {
    Serial.println("LoRa begin failed");
    while (true); // Drop into endless loop requiring restart
  }

  // Need to do this so field gateway pays attention to messsages from this device
  LoRa.enableCrc();
  LoRa.setSyncWord(FieldGatewaySyncWord);

#ifdef DEBUG_LORA
  LoRa.dumpRegisters(Serial);
#endif
  Serial.println("LoRa Setup done.");

  // Configure the Seeedstudio CO2, temperature & humidity sensor
  Serial.println("SCD30 setup start");
  Wire.begin();
  scd30.initialize();  
  delay(100);
  Serial.println("SCD30 setup done");

  PayloadHeader((byte *)FieldGatewayAddress,strlen(FieldGatewayAddress), deviceSerialNumber, DeviceSerialNumberLength);

  Serial.println("Setup done");
  Serial.println();
}


void loop()
{
  unsigned long currentMilliseconds = millis();  
  float temperature ;
  float humidity ;
  float co2;

  Serial.println("Loop called");

  if(scd30.isAvailable())
  {
    float result[3] = {0};
    PayloadReset();

    // Read the CO2, temperature & humidity values then display nicely
    scd30.getCarbonDioxideConcentration(result);

    co2 = result[0];
    Serial.print("C:");
    Serial.print(co2, 1) ;
    Serial.println("ppm ") ;

    PayloadAdd( "C", co2, 1, false);
    
    temperature = result[1];
    Serial.print("T:");
    Serial.print(temperature, 1) ;
    Serial.println("C ") ;

    PayloadAdd( "T", temperature, 1, false);

    humidity = result[2];
    Serial.print("H:" );
    Serial.print(humidity, 0) ;
    Serial.println("% ") ;

    PayloadAdd( "H", humidity, 0, true) ;

    #ifdef DEBUG_TELEMETRY
      Serial.println();
      Serial.print("RFM9X/SX127X Payload length:");
      Serial.print(payloadLength);
      Serial.println(" bytes");
    #endif

    LoRa.beginPacket();
    LoRa.write(payload, payloadLength);
    LoRa.endPacket();
  }
  Serial.println("Loop done");
  Serial.println();
  
  delay(SensorUploadDelay - (millis() - currentMilliseconds ));
}


void PayloadHeader( const byte *to, byte toAddressLength, const byte *from, byte fromAddressLength)
{
  byte addressesLength = toAddressLength + fromAddressLength ;

  payloadLength = 0 ;

  // prepare the payload header with "To" Address length (top nibble) and "From" address length (bottom nibble)
  
  payload[payloadLength] = (toAddressLength << 4) | fromAddressLength ;
  payloadLength += 1;

  // Copy the "To" address into payload
  memcpy(&payload[payloadLength], to, toAddressLength);
  payloadLength += toAddressLength ;

  // Copy the "From" into payload
  memcpy(&payload[payloadLength], from, fromAddressLength);
  payloadLength += fromAddressLength ;
}


void PayloadAdd( const char *sensorId, float value, byte decimalPlaces, bool last)
{
  byte sensorIdLength = strlen( sensorId ) ;

  memcpy( &payload[payloadLength], sensorId,  sensorIdLength) ;
  payloadLength += sensorIdLength ;
  payload[ payloadLength] = SensorIdValueSeperator;
  payloadLength += 1 ;
  payloadLength += strlen( dtostrf(value, -1, decimalPlaces, (char *)&payload[payloadLength]));
  if (!last)
  {
    payload[ payloadLength] = SensorReadingSeperator;
    payloadLength += 1 ;
  }
  
#ifdef DEBUG_TELEMETRY
  Serial.print("PayloadAdd float-payloadLength:");
  Serial.print( payloadLength);
  Serial.println( );
#endif
}


void PayloadAdd( char *sensorId, int value, bool last )
{
  byte sensorIdLength = strlen(sensorId) ;

  memcpy(&payload[payloadLength], sensorId,  sensorIdLength) ;
  payloadLength += sensorIdLength ;
  payload[ payloadLength] = SensorIdValueSeperator;
  payloadLength += 1 ;
  payloadLength += strlen(itoa( value,(char *)&payload[payloadLength],10));
  if (!last)
  {
    payload[ payloadLength] = SensorReadingSeperator;
    payloadLength += 1 ;
  }
  
#ifdef DEBUG_TELEMETRY
  Serial.print("PayloadAdd int-payloadLength:" );
  Serial.print(payloadLength);
  Serial.println( );
#endif
}


void PayloadAdd( char *sensorId, unsigned int value, bool last )
{
  byte sensorIdLength = strlen(sensorId) ;

  memcpy(&payload[payloadLength], sensorId,  sensorIdLength) ;
  payloadLength += sensorIdLength ;
  payload[ payloadLength] = SensorIdValueSeperator;
  payloadLength += 1 ;
  payloadLength += strlen(utoa( value,(char *)&payload[payloadLength],10));
  if (!last)
  {
    payload[ payloadLength] = SensorReadingSeperator;
    payloadLength += 1 ;
  }
  
#ifdef DEBUG_TELEMETRY
  Serial.print("PayloadAdd uint-payloadLength:");
  Serial.print(payloadLength);
  Serial.println( );
#endif
}


void PayloadReset()
{
  byte fromAddressLength = payload[0] & 0xf ;
  byte toAddressLength = payload[0] >> 4 ;
  
  payloadLength = toAddressLength + fromAddressLength + 1;
}


void DisplayHex( byte *byteArray, byte length) 
{
  for (int i = 0; i < length ; i++)
  {
    // Add a leading zero
    if ( byteArray[i] < 16)
    {
      Serial.print("0");
    }
    Serial.print(byteArray[i], HEX);
    if ( i < (length-1)) // Don't put a - after last digit
    {
      Serial.print("-");
    }
  }
}    

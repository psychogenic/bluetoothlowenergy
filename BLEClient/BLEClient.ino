/*
**  Bluetooth Smart/Low Energy Universal Client example/tutorial.
**  Copyright (C) 2014 Pat Deegan.  All rights reserved.
**
** http://www.flyingcarsandstuff.com/projects/Bluetooth
** 
** Please let me know if you use this code in your projects, and
** provide a URL if you'd like me to link to it from the project
** home.
**
** This program is free software;
** you can redistribute it and/or modify it under the terms of
** the GNU General Public License as published by the
** Free Software Foundation; either version 3 of the License,
** or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of 
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** See file LICENSE_GPL.txt for further informations on licensing terms.
**
**
** ************************* OVERVIEW ************************* 
**
** This program acts as a universal client for communicating 
** with BLE (Bluetooth Low Energy/Bluetooth Smart/Bluetooth 4.0)
** devices through the Bluegiga BLE112/BLE113 modules, using th
** BGLib library.
**
**
** The program should work on any Arduino connected to a 
** BLE11X module. Because a Serial port is needed for both
** the BLE module and the SerialUI user interface, ATmega328
** based Arduino's need to supplement the single (hardware) 
** Serial port with a second software serial.  Whether the 
** BLE module or the SerialUI<->USB interface is connected to 
** the software serial is set using the 
** USE_HARDWARE_SERIAL_FOR_BLE define, below.
** 
** 
** ********************** REQUIREMENTS **********************
** 
** You will need an Arduino and a Bluegiga BLE11X module (a 
** DIY breakout board project is detailed on
**   http://flyingcarsandstuff.com/projects/Bluetooth ) and 
** the following libraries:
**
**  BGLib (from bluegiga)
**  SerialUI (from http://flyingcarsandstuff.com/projects/SerialUI)
**
**
** ************************** USAGE **************************
**
** To use, 
**
** The code is fully commented, see below.
** Enjoy!
** Pat Deegan
*/

#include <BGLib.h>
#include <SerialUI.h>
#include <SoftwareSerial.h>

#include "Defs.h"


#if  not ( defined(SERIALUI_VERSION_AT_LEAST) and SERIALUI_VERSION_AT_LEAST(1, 8))
#error "You need to install SerialUI version 1.8 (or later) from http://flyingcarsandstuff.com/"
#endif





// define BLE112_PACKETMODE according to your BLE11X firmware (0 for "off" or 1 for "use packet mode")
#define BLE112_PACKETMODE	1

// if you've configured your BLE11X to sleep/wakeup with a pin
// uncomment the line below and set the BLE_WAKEUP_PIN (on the arduino)
// accordingly:
// #define USE_HARDWARE_WAKEUP_BEFORE_TX


#define LED_PIN         13          // Arduino LED
#define BLE_WAKEUP_PIN  4          // Assuming digital pin 4 is connected to BLE wake-up pin (needs 
#define BLE_RESET_PIN   5          // Assuming digital pin 5 is connected to BLE reset (with a pull-up)


#define BLE_UART_BAUD		9600
#define SERIALUI_PORT_BAUD	19200



// Pins used for software serial -- not all
// pins work for RX, choose wisely or leave as-is
#define SOFTWARE_SERIAL_RX_PIN    10
#define SOFTWARE_SERIAL_TX_PIN    11


// if you want HardwareSerial -> BLE, leave this 
// defined.  If you want HardwareSerial-> SerialUI, 
// comment it out (BLE will be connected through software 
// serial which may be unreliable, at least on a breadboard)
// 
#define USE_HARDWARE_SERIAL_FOR_BLE



/* Some defines used to create the global arrays we have
** to store context and attribute data.
** If you overdo this (make any value too large) you will 
** run out of RAM during execution, with undefined results.
*/

// ATTRIBDESC_MAX_BYTES max bytes save per attribute description
#define ATTRIBDESC_MAX_BYTES		        10


// MAX_NUM_UUIDS max number of attributes we'll consider
// (will cost you at least 28+ATTRIBDESC_MAX_BYTES bytes, each!)
#define MAX_NUM_UUIDS				14



// MAX_NUM_ATTRIB_GROUPS max number of services we'll query
#define MAX_NUM_ATTRIB_GROUPS		        5


// max addresses to hold while scanning...
// increase if there are lots of Bluetooth Smart devices in
// your environment
#define MAX_NUM_DEVICE_ADDRESSES	5



// debug output: may mess with BLE interaction timing if 
// you do too much on the SoftwareSerial line
//#define OUTPUT_DEBUG

// FTDI_DIRECT_CONNECT -- I set things up to be able
// to connect my USB<->FTDI/Serial thing directly to
// a Sippino on a breadboard.  Only define this if you
// have a clue what it entails (don't want to fry 
// your 'duino), and do a ctrl+f to find what I'm 
// doing with it below.
// #define FTDI_DIRECT_CONNECT



#ifdef USE_HARDWARE_SERIAL_FOR_BLE
  // Hardware serial connected to BLE, software serial to SerialUI terminal
  SoftwareSerial suiSoftSerial(SOFTWARE_SERIAL_RX_PIN, SOFTWARE_SERIAL_TX_PIN);
  #define BLESerialPortPointer    &Serial
  #define SUISerialPortPointer    &suiSoftSerial
#else
  // Hardware serial connected to SerialUI/Serial Monitor, software serial connected to BLE
  SoftwareSerial bleSerialPort(SOFTWARE_SERIAL_RX_PIN, SOFTWARE_SERIAL_TX_PIN);
  #define  BLESerialPortPointer    &bleSerialPort
  #define  SUISerialPortPointer    &Serial
#endif




// our strings for use with SerialUI -- stored in progmem
SUI_DeclareString(device_greeting,
                  "+++ Welcome to the BGTester +++\r\nEnter '?' to list options.");

SUI_DeclareString(top_menu_title, "BGTester Main");

SUI_DeclareString(reset_key, "reset");
SUI_DeclareString(reset_help, "Reset device");

SUI_DeclareString(hello_key, "hello");
SUI_DeclareString(hello_help, "Say hello/ping");


SUI_DeclareString(scan_key, "scan");
SUI_DeclareString(scan_help, "Scan for device addresses");

SUI_DeclareString(connect_key, "conn");
SUI_DeclareString(connect_help, "Connect to BLE");

SUI_DeclareString(listattribs_key, "list");
SUI_DeclareString(listattribs_help, "List all attributes found");


SUI_DeclareString(refresh_key, " ");
SUI_DeclareString(refresh_help, "Refresh BLE (check incoming messages)");


SUI_DeclareString(readattribs_key, "r attr");
SUI_DeclareString(readattribs_help, "Read an attribute");

SUI_DeclareString(writeattribs_key, "w attr");
SUI_DeclareString(writeattribs_help, "Write string to attribute");

SUI_DeclareString(writebytesattribs_key, "wbytes");
SUI_DeclareString(writebytesattribs_help, "Write bytes to attribute");


// messages and errors
SUI_DeclareString(error_timeout, "Timeout occurred!");
SUI_DeclareString(msg_hello_response, "<-- system_hello");
SUI_DeclareString(error_address_overflow, "Scan address list overflow--can't store:");
SUI_DeclareString(msg_connecting, "Making connection request");
SUI_DeclareString(msg_connected_to, "Connected to: ");
SUI_DeclareString(msg_selected_index, "Selected index: ");
SUI_DeclareString(msg_avail_attribs, "Available attributes: ");
SUI_DeclareString(error_invalid_selection, "Invalid selection, aborting.");
SUI_DeclareString(msg_select_address, "Select address of device: ");
SUI_DeclareString(error_runscan_first, "No device addresses to select from.  Run scan first...");
SUI_DeclareString(msg_scan_found_addresses, "Scan located following addresses:");
SUI_DeclareString(error_nothing_found_in_scan, "No devices found during scan");
SUI_DeclareString(msg_ble_reset, "-->\tsystem_reset: { boot_in_dfu: 0 }");
SUI_DeclareString(msg_saying_hello, "-->\tsystem_hello");
SUI_DeclareString(error_couldnotadd_command, "Could not addCommand to menu?");
SUI_DeclareString(error_ble_no_response, "BLE112 never responded");



// UUID_MAX_LEN_BYTES -- i wouldn't touch this...
#define UUID_MAX_LEN_BYTES			16
#define NUM_BYTES_IN_ADDRESS                    6


// A few defines to make checking for BLE activity a little simpler
#define DEFAULT_WAIT_TIMEOUT	        1500
#define SHORT_WAIT_TIMEOUT		700
#define WAIT_FOR_BLE_READY(tmout)		while ((ble112.checkActivity(tmout)))

#define WAIT_AND_CHECK_BLE(waittime)		delay(waittime); ble112.checkActivity()

#define CHECK_BLE_REPEATEDLY(dly, numtimes)		for(uint16_t __ho=0; __ho<numtimes; __ho++) { delay(dly); ble112.checkActivity(); }





/* *************************  Globals ************************* */

/* BGLib  global */
BGLib ble112((HardwareSerial *)BLESerialPortPointer, 0, BLE112_PACKETMODE);

/* SerialUI global */
SUI::SerialUI mySUI(device_greeting, 0, (HardwareSerial *)SUISerialPortPointer);


// restrict_to_sender_address -- will hold our slave's addy
uint8_t restrict_to_sender_address[NUM_BYTES_IN_ADDRESS] = {0};

// device_addresses_scanned -- will hold the advertisement addys encountered
uint8_t device_addresses_scanned[MAX_NUM_DEVICE_ADDRESSES][NUM_BYTES_IN_ADDRESS];
uint8_t num_device_address_scanned = 0;

#define UUID_STANDARD_ARRAY_LEN		2
uint8_t uuid_service[UUID_STANDARD_ARRAY_LEN] = { 0x00, 0x28}; // 0x2800 little end




/*
 *  ************************ State and housekeeping ************************
 *
 *
 * In this section, I'll define everything related to a little state machine and some helper functions and
 * structures used to gather info and maintain state.
 *
 * It's kinda messy, but it does the job.
 */
 
 
#define MAX_RESPONSE_LEN		20

// attribute access control flags
#define ACCESS_NONE		0
#define ACCESS_READ		0x02
#define ACCESS_WRITE	        0x08

// the various states we'll be going through
typedef enum mystateenum {
  StateStandby,
  StateAdvertiserSearch,
  StateConnecting,
  StateConnected,
  StateQueryingGroups,
  StateFindingAttribs,
  StateGettingAttribProperties,
  StateGettingAttribNames,
  StateAttribsAvailable
} MyState;


// Attribute details -- stuff related to a specific characteristic of interest
typedef struct attribdetails {
  bool discovered;
  uint8_t uuid[UUID_MAX_LEN_BYTES];
  uint8_t description[ATTRIBDESC_MAX_BYTES + 1]; // add a space for null termination
  uint8_t uuid_len;
  uint16_t chrhandle;
  uint16_t namehandle;
  uint16_t detshandle;
  uint8_t access_rights;
  uint8_t connection;
  bool success;

  attribdetails() : discovered(false), uuid_len(0), chrhandle(0), namehandle(0),
    access_rights(ACCESS_NONE), connection(69), success(false)
  {

  }
} AttribDets;

// attribute groups (services)
typedef struct attrGroupBoundsStruct {
  uint16_t att_start;
  uint16_t att_end;
  attrGroupBoundsStruct() : att_start(0), att_end(0) {}

} AttribGroupBounds;

// create a single buffer large enough to hold any attrib
// data we'll read or write to, so we can re-use it in functions
// without messing with the stack
uint8_t data_xchange_buffer[MAX_RESPONSE_LEN + 1];



// the global context, in which we keep track of all the attributes
// and our various counters used when chaining read requests and such
typedef struct mycontextstruct {
  MyState currentState;
  bool ble_responded;
  uint8_t connection_id;
  AttribGroupBounds attrib_groups[MAX_NUM_ATTRIB_GROUPS];
  AttribDets attrib_details[MAX_NUM_UUIDS];
  
  // uint8_t nameAttribPointerHandles[MAX_NUM_UUIDS];

  uint8_t cur_num_attribs;
  uint8_t cur_num_groups;
  // uint8_t cur_num_namepointer_handles;
  uint8_t num_groups_queried;
  // uint8_t num_namepointers_queried;
  uint8_t num_names_queried;
  uint8_t num_properties_queried;

  mycontextstruct() : currentState(StateStandby), cur_num_attribs(0), cur_num_groups(0),
    num_groups_queried(0),  num_names_queried(0), num_properties_queried(0) {}


  int8_t registerGroup(const uint8array & uuid, uint16_t start, uint16_t end)
  {

    int8_t groupIdx = -1;
    if (cur_num_groups < MAX_NUM_ATTRIB_GROUPS)
    {
      // still have space...
      groupIdx = cur_num_groups;
      cur_num_groups++;
      attrib_groups[groupIdx].att_start = start;
      attrib_groups[groupIdx].att_end = end;

    }

    return groupIdx;
  }
  int8_t registerUUID(const uint8array & uuid)
  {
    int8_t attribIdx = -1;
    if (cur_num_attribs >= MAX_NUM_UUIDS) {
      mySUI.println(F("Out of space for attribs!"));
    } else {

      attribIdx = cur_num_attribs;
      cur_num_attribs++;
      // mySUI.print(F(" *** Registering UUID ***"));
      // mySUI.println(attribIdx);
      attrib_details[attribIdx].uuid_len = uuid.len;

      for (uint8_t i = 0; i < uuid.len; i++) {
        attrib_details[attribIdx].uuid[i] = uuid.data[i];
        //	mySUI.print(attrib_details[attribIdx].uuid[i], HEX);
      }


    }

    return attribIdx;
  }

  bool haveValidNameAttrib()
  {
    while (num_names_queried < cur_num_attribs
           && attrib_details[num_names_queried].namehandle == 0)
      num_names_queried++; // skip any attribs without descriptors

    return  (num_names_queried < cur_num_attribs) ;
  }

  bool haveValidPropertiesAttrib()
  {
    while (num_properties_queried < cur_num_attribs
           && attrib_details[num_properties_queried].detshandle == 0)
      num_properties_queried++; // skip any attribs without descriptors

    return  (num_properties_queried < cur_num_attribs) ;
  }


} MyContext;

MyContext context;



// a check for a given UUID, to see if we've already
// recorded it in the context.
int knownUUID(const uint8_t  * check, uint16_t handle = 0)
{

  for (uint8_t attr_idx = 0; attr_idx < MAX_NUM_UUIDS; attr_idx++)
  {
    uint8_t i = 0;

    if (! context.attrib_details[attr_idx].uuid_len)
      continue;

    for (i = 0; i < context.attrib_details[attr_idx].uuid_len; i++)
    {
      // if (check[i] != context.service_littleendian_keys[j][i])
      if (check[i] != context.attrib_details[attr_idx].uuid[i])
      {
        // not a match
        break; // break out before the end...
      }
    }
    if (i >= context.attrib_details[attr_idx].uuid_len) {

      if (! handle)
      {
        // we're good, no handle comparison needed
        mySUI.print(F("known UUID FOUND! "));
        mySUI.println(attr_idx, DEC);
        return attr_idx;
      }
      // need to compare
      if ((!context.attrib_details[attr_idx].chrhandle)
          || context.attrib_details[attr_idx].chrhandle == handle) {
        // Passed!
#ifdef OUTPUT_DEBUG
        mySUI.print(F("known UUID FOUND! "));
        mySUI.println(attr_idx, DEC);
#endif
        return attr_idx;
      } else {

#ifdef OUTPUT_DEBUG
        mySUI.print(F("UUID match but handle mismatch ("));
        mySUI.print(handle, DEC);
        mySUI.print(F("!="));
        mySUI.print(context.attrib_details[attr_idx].chrhandle, DEC);
        mySUI.println(')');
#endif

      }

    }


  }


  return -1;

}


// a little function to get the descriptor registered for an attribute
const char * getNameFor(uint8_t attribIdx)
{
  if (context.attrib_details[attribIdx].description[0])
    return (const char*)(context.attrib_details[attribIdx].description);

  return NULL;

}

// a way to lookup attributes based on their handle
int attribByHandle(uint16_t chrhandle)
{
  for (int i = 0; i < MAX_NUM_UUIDS; i++)
  {
    if (chrhandle == context.attrib_details[i].chrhandle)
      return i;
  }

  return -1;
}

// a utility method to output a (BGLib-defined) uint8array
void outputArray(const uint8array * a)
{
  //

  mySUI.print(F("     "));
  for (uint8_t i = 0; i < a->len; i++)
  {

    if (a->data[i] >= ' ' && a->data[i] <= 'z')
    {
      mySUI.print((char)a->data[i]);
    } else {
      mySUI.print(a->data[i], HEX);
    }
    mySUI.print(' ');
  }
  mySUI.println(' ');
  mySUI.print(F("HEX: "));
  for (uint8_t i = 0; i < a->len; i++)
  {
    mySUI.print(a->data[i], HEX);

    mySUI.print(' ');
  }
  mySUI.println(' ');
}





/*
 *  ************************ BGLib callbacks ************************
 *
 *  These methods are configured (in setup()) to be used as callbacks
 *  for various BGLib responses/events.
 */

// internals, called by the lib when internal stuff happens
void onBusy() {
  // turn LED on when we're busy

  digitalWrite(LED_PIN, HIGH);
}

void onIdle() {
  // turn LED off when we're no longer busy
  //
  digitalWrite(LED_PIN, LOW);
}

void onTimeout() {
  mySUI.println_P(error_timeout);
}



#ifdef USE_HARDWARE_WAKEUP_BEFORE_TX
void onBeforeTXCommand() {
  // wake module up (assuming here that digital pin 5 is connected to the BLE wake-up pin)
  digitalWrite(BLE_WAKEUP_PIN, HIGH);

  // wait for "hardware_io_port_status" event to come through, and parse it (and otherwise ignore it)
  uint8_t *last; u
  while (1) {
    ble112.checkActivity();
    last = ble112.getLastEvent();
    if (last[0] == 0x07 && last[1] == 0x00) break;
  }

  // give a bit of a gap between parsing the wake-up event and allowing the command to go out
  delayMicroseconds(600);
}

void onTXCommandComplete() {
  // allow module to return to sleep (assuming here that digital pin 5 is connected to the BLE wake-up pin)
  digitalWrite(BLE_WAKEUP_PIN, LOW);
}

#endif


// response callbacks -- called when the BGLib responds to one of our requests
void my_rsp_system_hello(const ble_msg_system_hello_rsp_t *msg) {
  context.ble_responded = true;
  mySUI.println_P(msg_hello_response);
}

void my_rsp_gap_set_scan_parameters(const ble_msg_gap_set_scan_parameters_rsp_t *msg) {

  context.ble_responded = true;
#ifdef OUTPUT_DEBUG
  mySUI.print(F("<--\tgap_set_scan_parameters: { "));
  mySUI.print(F("result: ")); mySUI.print((uint16_t)msg -> result, HEX);
  mySUI.println(F(" }"));
#endif
}

void my_rsp_gap_discover(const ble_msg_gap_discover_rsp_t *msg) {

  context.ble_responded = true;

#ifdef OUTPUT_DEBUG
  mySUI.print(F("<--\tgap_discover: { "));
  mySUI.print(F("result: ")); mySUI.print((uint16_t)msg -> result, HEX);
  mySUI.println(F(" }"));
#endif

}

void my_rsp_gap_end_procedure(const ble_msg_gap_end_procedure_rsp_t *msg) {

  context.ble_responded = true;
#ifdef OUTPUT_DEBUG
  mySUI.print(F("<--\tgap_end_procedure: { "));
  mySUI.print(F("result: ")); mySUI.print((uint16_t)msg -> result, HEX);
  mySUI.println(F(" }"));
#endif
}



// ******** Event callbacks **********
// similar to response callbacks, but can be called "any time"

void my_evt_gap_scan_response(const ble_msg_gap_scan_response_evt_t *msg) {

  //

  if (context.currentState == StateAdvertiserSearch)
  {
    // we're currently scanning to generate a list of advertisers in
    // the vicinity

    // check if we already know the msg->sender
    uint8_t addridx;
    for (addridx = 0; addridx < num_device_address_scanned; addridx++)
    {

      uint8_t addrbyte = 0;
      while (addrbyte < 6 && (device_addresses_scanned[addridx][addrbyte] == msg->sender.addr[addrbyte]))
      {
        addrbyte++;
      }

      if (addrbyte >= 6) // all bytes matched
        break; // found it in already scanned list

    }

    if (addridx < num_device_address_scanned)
    {
      // yep, it was already in list
      // skip it...
      return;
    }

    // ok, we should add it to list if we can.
    if (num_device_address_scanned >= (MAX_NUM_DEVICE_ADDRESSES - 1))
    {
      // no space left for this addy...
      mySUI.print_P(error_address_overflow);
      for (uint8_t i = 0; i < NUM_BYTES_IN_ADDRESS; i++)
        mySUI.print(msg->sender.addr[i], HEX);
      mySUI.println(' ');
      return;
    }
#ifdef OUTPUT_DEBUG
    mySUI.print(F("Scan found address: "));
#endif
    for (uint8_t i = 0; i < NUM_BYTES_IN_ADDRESS; i++)
    {

#ifdef OUTPUT_DEBUG
      mySUI.print(msg->sender.addr[i], HEX);
#endif
      device_addresses_scanned[num_device_address_scanned][i] = msg->sender.addr[i];

    }
#ifdef OUTPUT_DEBUG
    mySUI.println(' ');
#endif
    num_device_address_scanned++;
    return;

  }

  if (context.currentState != StateStandby)
  {
    // we're not looking for the device/service anymore, drop this like it's hot
    return;
  }

  // check sender address
  for (uint8_t addrbyte = 0; addrbyte < NUM_BYTES_IN_ADDRESS; addrbyte++)
  {
    if (restrict_to_sender_address[addrbyte] && (restrict_to_sender_address[addrbyte] != msg->sender.addr[addrbyte]))
    {
#ifdef OUTPUT_DEBUG
      mySUI.println(F("Ignoring scan response from :"));
      for (uint8_t i = 0; i < 6; i++) {
        mySUI.print(msg->sender.addr[i], HEX);
        mySUI.print(' ');
      }

      mySUI.print(F("Because "));
      mySUI.print(restrict_to_sender_address[addrbyte], HEX);
      mySUI.print(F(" != "));
      mySUI.println(msg->sender.addr[addrbyte], HEX);
#endif
      return;
    }
  }

#ifdef OUTPUT_DEBUG
  mySUI.print(F("###\tgap_scan_response: { "));
  mySUI.print(F("rssi: ")); mySUI.print(msg -> rssi);
  mySUI.print(F(", packet_type: ")); mySUI.print((uint8_t)msg -> packet_type, HEX);
  mySUI.print(F(", sender: "));
  // this is a "bd_addr" data type, which is a 6-byte uint8_t array
  for (uint8_t i = 0; i < 6; i++) {
    if (msg -> sender.addr[i] < 16) mySUI.write('0');
    mySUI.print(msg -> sender.addr[i], HEX);
  }
  mySUI.print(F(", address_type: ")); mySUI.print(msg -> address_type, HEX);
  mySUI.print(F(", bond: ")); mySUI.print(msg -> bond, HEX);
  mySUI.print(F(", data: "));
  // this is a "uint8array" data type, which is a length byte and a uint8_t* pointer
  for (uint8_t i = 0; i < msg -> data.len; i++) {
    if (msg -> data.data[i] < 16) mySUI.write('0');
    mySUI.print(msg -> data.data[i], HEX);
  }
  mySUI.println(F(" }"));
#endif

  // we *do* want to connect to this guy...

  context.currentState = StateConnecting;  // note that we're now connecting
  mySUI.println_P(msg_connecting);

  // actually make the connection request
  ble112.ble_cmd_gap_connect_direct(msg->sender, msg->address_type, 0x06,
                                    0x0c, 0x200, 0);


}


// connection status gets called after we've made the connection request (above)
void my_ble_evt_connection_status(const struct ble_msg_connection_status_evt_t *msg)
{

#ifdef OUTPUT_DEBUG
  mySUI.println(F("my_ble_evt_connection_status"));
#endif

  if ( (msg->flags & 0x05) != 0x05)
  {
    mySUI.print(F("Seems like connection failed. flags:"));
    mySUI.println(msg->flags, HEX);

    return;
  }
  // looks like we managed to connect... yay!
  mySUI.println(F("Huzzah! Connected!"));
  mySUI.print_P(msg_connected_to);
  for (uint8_t i = 0; i < NUM_BYTES_IN_ADDRESS; i++)
  {
    mySUI.print(msg->address.addr[i], HEX);
    if (i < 5)
      mySUI.print(':');
  }


  context.connection_id = msg->connection;

  // update our little "state machine" for the next step
  context.currentState = StateQueryingGroups;

  context.num_groups_queried = 0; // reset our counter

  ble112.ble_cmd_attclient_read_by_group_type(msg->connection, 0x0001, 0xFFFF, UUID_STANDARD_ARRAY_LEN,
      uuid_service);


}

void my_ble_evt_attclient_group_found(const struct ble_msg_attclient_group_found_evt_t *msg)
{
#ifdef OUTPUT_DEBUG
  mySUI.println(F("my_ble_evt_attclient_group_found for "));

  outputArray(&(msg->uuid));

  mySUI.print(F("with bounds ["));
  mySUI.print(msg->start, DEC);
  mySUI.print('-');
  mySUI.print(msg->end, DEC);
  mySUI.println(']');
#endif

  // a group we'll want to learn more about...
  context.registerGroup(msg->uuid, msg->start, msg->end);

}


// we define a few handlers used below for characteristics with specific, pre-defined, UUIDs


void nullHandler(const struct ble_msg_attclient_find_information_found_evt_t *msg)
{
  // just want to ignore whatever we send here...
}

void linkHandler(const struct ble_msg_attclient_find_information_found_evt_t *msg)
{
  // this attribute points to the "next" attribute we'll hit
  // we make note of it's own handle so we can query it later.
  context.attrib_details[context.cur_num_attribs].detshandle = msg->chrhandle;
}

void nameContentHandler(const struct ble_msg_attclient_find_information_found_evt_t *msg)
{
  // this is a descriptor attribute, for the last encountered characteristic,
  // we stash its handle so we can query it later
  if (context.cur_num_attribs)
    context.attrib_details[context.cur_num_attribs - 1].namehandle = msg->chrhandle;

}


standardUUIDInfoHandler hasHandler(const uint8array &  uuidArray)
{

  // for certain characteristics (with standard 2-byte UUIDs) we'll
  // do some processing later... just what, depends on the type so
  // this function either returns a handler to do just that, or
  // NULL if we want to leave processing to somewhere else
  if (uuidArray.len != 2)
    return NULL; // not of interest, some custom characteristic

  if (uuidArray.data[0] == 0x03 && uuidArray.data[1] == 0x28)
    return linkHandler; // contains the link between attrib handle and uuid, and info about access properties

  if (uuidArray.data[0] == 0x01 && uuidArray.data[1] == 0x29)
    return nameContentHandler; // contains the attrib description -- will deal with later

  if (uuidArray.data[0] == 0x0 && uuidArray.data[1] == 0x28)
    return nullHandler; // the start of our chain, don't care

  // by default, play safe and leave it be
  return NULL;
}


// our callback for info found
void my_ble_evt_attclient_find_information_found(const struct ble_msg_attclient_find_information_found_evt_t *msg)
{


#ifdef OUTPUT_DEBUG
  mySUI.println(F("my_ble_evt_attclient_find_information_found"));
#endif

  // see if we have a handler setup for whatever this is
  standardUUIDInfoHandler attribHandler = hasHandler(msg->uuid);
  if (attribHandler)
  {
    // yep, just let that handler deal with it
    attribHandler(msg);
    return;
  }

  // check if we already know this UUID
  int8_t idx = knownUUID(msg->uuid.data, msg->chrhandle);
  if (idx < 0)
  {
    // don't know this UUID, make a note of it
    idx = context.registerUUID(msg->uuid);
  }
  if (idx >= 0)
  {
    // it's registered in our list now, keep track of its handle and such
#ifdef OUTPUT_DEBUG
    mySUI.println(F("Found handle for an interesting characteristic!"));
    mySUI.println(idx, DEC);
#endif

    context.attrib_details[idx].discovered = true;
    context.attrib_details[idx].chrhandle = msg->chrhandle;
    context.attrib_details[idx].connection = msg->connection;

  }

}

// called when we've asked for an attribute value and the
// remote side has gotten back to us
void my_ble_evt_attclient_attribute_value(
  const struct ble_msg_attclient_attribute_value_evt_t *msg) {




  // if we get here, it's because we're in the StateAttribsAvailable state,
  // we've made a request for an attributes value, and got a response back
  // that has triggered this callback.

#ifdef OUTPUT_DEBUG
  mySUI.print(F("Attrib value "));
#endif

  int8_t attribIdx = attribByHandle(msg->atthandle);
  if (attribIdx >= 0) {

    const char * attrName = getNameFor(attribIdx);
    if (attrName) {
      mySUI.println(attrName);
    } else {
      mySUI.print(F("Attribute "));
      mySUI.println(attribIdx, DEC);
    }

    context.attrib_details[attribIdx].success = true;

    uint8_t len =
      msg->value.len < MAX_RESPONSE_LEN ?
      msg->value.len : MAX_RESPONSE_LEN;
    for (uint8_t i = 0; i < len; i++) {
      data_xchange_buffer[i] = msg->value.data[i];
    }
  } else {
    mySUI.println(F("UNKNOWN!"));
  }

  outputArray(&(msg->value));


}




// called when we've asked for an attribute value
// while were getting attribute properties
void my_ble_attclient_attribute_value_gettingAttribProps (
  const struct ble_msg_attclient_attribute_value_evt_t *msg) {



  if (context.currentState != StateGettingAttribProperties) {

    mySUI.println(F("Weird: shouldn't be in attrvalue_gettingAttribNames"));
    return;
  }


  // we're getting attribute (read/write) properties and have a result.
#ifdef OUTPUT_DEBUG
  mySUI.println(F("Got a char property ****** "));
  outputArray(&(msg->value));
#endif

  // clear out any settings for the relevant attribs access rights
  context.attrib_details[context.num_properties_queried].access_rights =
    ACCESS_NONE;

  // set read right, if applicable
  if (msg->value.len && (msg->value.data[0] & ACCESS_READ))
    context.attrib_details[context.num_properties_queried].access_rights |=
      ACCESS_READ;

  // set write right, if applicable
  if (msg->value.len && (msg->value.data[0] & ACCESS_WRITE))
    context.attrib_details[context.num_properties_queried].access_rights |=
      ACCESS_WRITE;

  // move on to the next attrib with a properties (dets) handle set, if possible
  context.num_properties_queried++;
  if (context.haveValidPropertiesAttrib()) {
    // yep, have more... read the next one
    ble112.ble_cmd_attclient_read_by_handle(context.connection_id,
                                            context.attrib_details[context.num_properties_queried].detshandle);

    // were done here for now
    return;

  }

  // seems we're done with all properties...
  // finally ready to go!
  context.currentState = StateAttribsAvailable;

  // we set the attribute value event handler to the one used
  // while "reading characteristics"
  ble112.ble_evt_attclient_attribute_value =
    my_ble_evt_attclient_attribute_value;


  return;
}


// called when we've asked for an attribute value
// while were getting attribute names
void my_attclient_attribute_value_gettingAttribNames (
  const struct ble_msg_attclient_attribute_value_evt_t *msg) {




  if (context.currentState != StateGettingAttribNames) {

    mySUI.println(F("Weird: shouldn't be in attrvalue_gettingAttribNames"));
    return;
  }

  // we're currently getting the attribute names...
  // this value will be the name of the attribute at index
  // context.num_namepointers_queried
#ifdef OUTPUT_DEBUG
  mySUI.println(F("Got NAME ****** "));

  outputArray(&(msg->value));
#endif
  uint8_t maxlen =
    msg->value.len < ATTRIBDESC_MAX_BYTES ?
    msg->value.len : ATTRIBDESC_MAX_BYTES;
  uint8_t i;
  for (i = 0; i < maxlen; i++) {
    context.attrib_details[context.num_names_queried].description[i] =
      msg->value.data[i];
  }
  context.attrib_details[context.num_names_queried].description[i] = 0; // null terminate the str.

  // move to the next name, if we can
  context.num_names_queried++;
  if (context.haveValidNameAttrib()) {
    // get the next one by making another read request
    ble112.ble_cmd_attclient_read_by_handle(context.connection_id,
                                            context.attrib_details[context.num_names_queried].namehandle);

    // we're done here for now
    return;
  }

  // looks like we're done with the names, now.
#ifdef OUTPUT_DEBUG
  mySUI.println(F("** GOT ALL NAMES **"));
  mySUI.println(F("Now getting access props"));
#endif
  // now we want to make some additional queries to figure out the
  // availability of read/write for every attribute in our little list.
  context.currentState = StateGettingAttribProperties;

  // we set the attribute value event handler to the one used during
  // attrib properties fetching
  ble112.ble_evt_attclient_attribute_value =
    my_ble_attclient_attribute_value_gettingAttribProps;

  // try to setup the first attrib with a "details handle" set
  context.num_properties_queried = 0;
  if (context.haveValidPropertiesAttrib()) {

    // got one... issue a read by handle on that properties attribute, so we
    // can figure out access rights.  Subsequent calls will be chained
    // below, in the StateGettingAttribProperties state.
    ble112.ble_cmd_attclient_read_by_handle(context.connection_id,
                                            context.attrib_details[context.num_properties_queried].detshandle);
  }

  // ok, we're done here either way.
  return;
}




// the procedure completed gets called for a number of reasons, so we have a bunch
// of if/else acting like switches depending on our state machine's state
void my_ble_evt_attclient_procedure_completed(
  const struct ble_msg_attclient_procedure_completed_evt_t *msg) {



#ifdef OUTPUT_DEBUG
  mySUI.println(F("proc comp!"));
#endif

  if (msg->result)
    mySUI.println(F("But a prob encountered..."));

  if (context.currentState == StateQueryingGroups) {
    // ok, we're querying the groups to find whatever's in there
    if (context.num_groups_queried < context.cur_num_groups) {
      // stillhave groups to query

      mySUI.print(F("Requesting attclient info for group "));
      mySUI.println(context.num_groups_queried, DEC);


      // call the find info and wait to be called back
      ble112.ble_cmd_attclient_find_information(context.connection_id,
          context.attrib_groups[context.num_groups_queried].att_start,
          context.attrib_groups[context.num_groups_queried].att_end);

      // move on to next group
      context.num_groups_queried++;

      // we're done here
      return;

    } // end if we still had groups to query

    // if we get here, we're done querying the groups...
    // we'll now try and get names for everybody.

    context.num_names_queried = 0; // set it at start

    // ok, so now we're getting name handles... get the next one if available
    if (context.haveValidNameAttrib()) {
      mySUI.println(F("Getting the names now..."));

      context.currentState = StateGettingAttribNames; // keep our state machine in sync with what we're doing

      // we set the attribute value event handler to the one used
      // while "getting attribute names"
      ble112.ble_evt_attclient_attribute_value =
        my_attclient_attribute_value_gettingAttribNames;

      // make the first call to read a name attribute
      ble112.ble_cmd_attclient_read_by_handle(context.connection_id,
                                              context.attrib_details[context.num_names_queried].namehandle);

      // Subsequent calls to get the entire list will be chained in
      // the attribute value callback.

      // we're done here
      return;

    }  // end if we have names to query

    // if we get here, it means we
    // don't have any name attribs... ah well, fuggetaboudit.
    mySUI.println(F("No names to be found"));


    context.currentState = StateAttribsAvailable;

    // we set the attribute value event handler to the one used
    // while "getting characteristic values"
    ble112.ble_evt_attclient_attribute_value =
      my_ble_evt_attclient_attribute_value;





  } else if (context.currentState == StateAttribsAvailable) {

    // when we end up in this callback (procedure complete) while we're
    // in the "attribs available" state, it's because we did a read or write
    // on the thing.  Thus, we'll output a little feedback and make note
    // of the returned result in that attribute's "success" field.

    int attribIdx = attribByHandle(msg->chrhandle);
    const char * attrName = getNameFor(attribIdx);

    if (attrName) {
      mySUI.print(attrName);
      context.attrib_details[attribIdx].success =
        msg->result ? false : true;
    } else {
      mySUI.print(F("attribute index "));
      mySUI.print(attribIdx, DEC);
    }

    mySUI.println(msg->result ? " FAILURE" : " Success!");
  }

}


/*
 *  ******************************* SUI callback helpers *******************************
 *
 *  Our SerialUI callbacks use the following helper functions to get stuff done.
 *
 */


void cancelGapScanning()
{
  // disable any scanning
  ble112.ble_cmd_gap_set_mode(0, 0);
  ble112.ble_cmd_gap_end_procedure();
  ble112.checkActivity();
}


// output a list of characteristics, potentially restricted to those
// with certain access rights
void outputListOfAttribs(uint8_t restrictTo = 0)
{
  for (uint8_t attridx = 0; attridx < context.cur_num_attribs; attridx++) {
    if (restrictTo && ! (context.attrib_details[attridx].access_rights & restrictTo))
      continue;

    mySUI.print(' ');
    mySUI.print(attridx + 1, DEC);
    mySUI.print(F(":\t"));

    const char * attrName = getNameFor(attridx);
    if (attrName) {
      mySUI.print(attrName);
      mySUI.print(F("  --  "));
    }

    for (uint8_t j = 0; j < context.attrib_details[attridx].uuid_len; j++)
    {
      if (j && (j % 2 == 0))
        mySUI.print('-');

      if (context.attrib_details[attridx].uuid[j] < 0x10)
        mySUI.print('0');

      mySUI.print(context.attrib_details[attridx].uuid[j], HEX);

    }

    mySUI.print(F(" ["));
    mySUI.print(context.attrib_details[attridx].chrhandle, DEC);
    mySUI.print(F("] "));

    if (context.attrib_details[attridx].access_rights & ACCESS_READ)
      mySUI.print('R');

    if (context.attrib_details[attridx].access_rights & ACCESS_WRITE)
      mySUI.print('W');


    mySUI.println(' ');

  }

}



// a method used to send a read_by_handle request and check for results
void readAnAttrib(uint8_t attribIdx)
{
  context.attrib_details[attribIdx].success = false;

  ble112.ble_cmd_attclient_read_by_handle(
    context.connection_id,
    context.attrib_details[attribIdx].chrhandle);


  CHECK_BLE_REPEATEDLY(25, 30);

  if (context.attrib_details[attribIdx].success)
    return mySUI.returnOK();

  return mySUI.returnError("Couldn't read attrib");

}


// a method to write some data to an attrib
void writeAnAttrib(uint8_t attribIdx, uint8_t len, uint8_t * data)
{
  mySUI.print(F("Attempting to write attrib len "));
  mySUI.println(len, DEC);

  context.attrib_details[attribIdx].success = false;

  ble112.ble_cmd_attclient_attribute_write(context.connection_id, context.attrib_details[attribIdx].chrhandle,
      len, data);


  CHECK_BLE_REPEATEDLY(25, 30);

  if (context.attrib_details[attribIdx].success)
    return mySUI.returnOK();

  return mySUI.returnError("Problem with write to attrib");

}




/*
 *  ******************************* SerialUI callbacks ****************************
 *
 * These are the methods called when you select a command from a SerialUI menu.
 *
 */


void resetBLE()
{
  mySUI.println_P(msg_ble_reset);

#ifdef BLE_RESET_PIN
  digitalWrite(BLE_RESET_PIN, LOW);
  delayMicroseconds(50);
  digitalWrite(BLE_RESET_PIN, HIGH);
#else
  ble112.ble_cmd_system_reset(0);
  WAIT_FOR_BLE_READY(DEFAULT_WAIT_TIMEOUT);
#endif

  // try to reset the SoftwareSerial port
  (BLESerialPortPointer)->begin(BLE_UART_BAUD);
  mySUI.returnOK();
}

void hello()
{

  context.ble_responded = false;
  mySUI.println_P(msg_saying_hello);
  ble112.ble_cmd_system_hello();
  WAIT_FOR_BLE_READY(DEFAULT_WAIT_TIMEOUT);

  if (! context.ble_responded )
    return mySUI.returnError_P(error_ble_no_response);

  mySUI.returnOK();
}

void startScanning()
{

  ble112.ble_cmd_connection_disconnect(0);
  cancelGapScanning();
  // setup scan
  ble112.ble_cmd_gap_set_scan_parameters(75, 50, 1);
  ble112.checkActivity(50);
  ble112.ble_cmd_gap_discover(1);

}
void scanBLE()
{



  context.ble_responded = false;

  mySUI.println(F("scanning"));
  num_device_address_scanned = 0;
  context.currentState = StateAdvertiserSearch;
  startScanning();

  CHECK_BLE_REPEATEDLY(10, 400);

  cancelGapScanning();

  if (! context.ble_responded)
    return mySUI.returnError_P(error_ble_no_response);


  if (! num_device_address_scanned)
    return mySUI.returnError_P(error_nothing_found_in_scan);


  mySUI.println_P(msg_scan_found_addresses);
  for (uint8_t i = 0; i < num_device_address_scanned; i++)
  {
    for (uint8_t addrbyte = 0; addrbyte < NUM_BYTES_IN_ADDRESS; addrbyte++)
      mySUI.print(device_addresses_scanned[i][addrbyte], HEX);

    mySUI.println(' ');

  }

}



void connectBLE()
{


#ifdef OUTPUT_DEBUG
  mySUI.println(F("connecting"));
#endif



  if (! num_device_address_scanned)
    return mySUI.returnError_P(error_runscan_first);

  mySUI.println_P(msg_select_address);
  for (uint8_t i = 0; i < num_device_address_scanned; i++)
  {
    mySUI.print(i + 1, DEC);
    mySUI.print(F(" :\t"));
    for (uint8_t addrbyte = 0; addrbyte < NUM_BYTES_IN_ADDRESS; addrbyte++)
      mySUI.print(device_addresses_scanned[i][addrbyte], HEX);

    mySUI.println(' ');
  }

  mySUI.showEnterNumericDataPrompt();
  int selection = (mySUI.parseInt() - 1);
  if (selection < 0 || selection >= num_device_address_scanned)
    return mySUI.returnError_P(error_invalid_selection);

  for (uint8_t addrbyte = 0; addrbyte < NUM_BYTES_IN_ADDRESS; addrbyte++)
    restrict_to_sender_address[addrbyte] = device_addresses_scanned[selection][addrbyte];


  context.currentState = StateStandby;
  ble112.ble_evt_attclient_attribute_value = my_ble_evt_attclient_attribute_value;
  startScanning();
  CHECK_BLE_REPEATEDLY(7, 500);

  outputListOfAttribs();

  mySUI.returnOK();
}

void listAttribs()
{

  mySUI.println(F("attributes found:"));

  outputListOfAttribs();

  mySUI.returnOK();
}




void doRead()
{
  mySUI.println_P(msg_avail_attribs);
  outputListOfAttribs(ACCESS_READ);

  mySUI.print_P(msg_selected_index);
  mySUI.showEnterNumericDataPrompt();

  int idx = mySUI.parseInt();
  if (idx < 1 || idx > context.cur_num_attribs)
    return mySUI.returnError_P(error_invalid_selection);


  readAnAttrib(idx - 1);
}

void doWrite()
{

  

  mySUI.println_P(msg_avail_attribs);

  outputListOfAttribs(ACCESS_WRITE);
  mySUI.print_P(msg_selected_index);

  mySUI.showEnterNumericDataPrompt();

  int idx = mySUI.parseInt();
  if (idx < 1 || idx > context.cur_num_attribs)
    return mySUI.returnError_P(error_invalid_selection);

  size_t len = mySUI.readBytesToEOL((char*)data_xchange_buffer, MAX_RESPONSE_LEN + 1);

  writeAnAttrib(idx - 1, len, data_xchange_buffer);



}

void doWriteBytes()
{
  mySUI.println_P(msg_avail_attribs);

  outputListOfAttribs(ACCESS_WRITE);
  mySUI.print_P(msg_selected_index);


  mySUI.showEnterNumericDataPrompt();

  int idx = mySUI.parseInt();
  if (idx < 1 || idx > context.cur_num_attribs)
    return mySUI.returnError_P(error_invalid_selection);

  uint8_t byteIdx = 0;
  while (byteIdx < MAX_RESPONSE_LEN)
  {
    int val = mySUI.parseInt();
    if (val < 0)
      break;

    data_xchange_buffer[byteIdx++] = val;
  }

  if (! byteIdx)
    return mySUI.returnError("Aborted");

  writeAnAttrib(idx - 1, byteIdx, data_xchange_buffer);


}


void refreshBLE()
{
  
  CHECK_BLE_REPEATEDLY(10, 70);
  mySUI.returnOK();
}

void setupMenus() {

  // Get a handle to the top level menu
  // Note that menus are returned as pointers.
  SUI::Menu * mainMenu = mySUI.topLevelMenu();
  if (!mainMenu) {
    // what? Could not create :
    return mySUI.returnError("Could not get top level menu?");

  }

  // we can set the name (title) of any menu using
  // setName().  This shows up in help output and when
  // moving up and down the hierarchy.  If it isn't set,
  // the menu key will be used, for sub-menus, and the default
  // SUI_SERIALUI_TOP_MENU_NAME ("TOP") for top level menus.
  mainMenu->setName(top_menu_title);

  // we'll keep it a simple 1-level menu -- just a bunch of commands.

  if (!mainMenu->addCommand(reset_key, resetBLE, reset_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  if (!mainMenu->addCommand(hello_key, hello, hello_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  if (!mainMenu->addCommand(scan_key, scanBLE, scan_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }
  if (!mainMenu->addCommand(connect_key, connectBLE, connect_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  if (!mainMenu->addCommand(listattribs_key, listAttribs, listattribs_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  if (!mainMenu->addCommand(readattribs_key, doRead, readattribs_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }
  if (!mainMenu->addCommand(writeattribs_key, doWrite, writeattribs_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }
  if (!mainMenu->addCommand(writebytesattribs_key, doWriteBytes,
                            writebytesattribs_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  if (!mainMenu->addCommand(refresh_key, refreshBLE, refresh_help)) {
    return mySUI.returnError_P(error_couldnotadd_command);
  }

  // done!
  return;
}



void setup() {

  // initialize status LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

#ifdef BLE_RESET_PIN
  pinMode(BLE_RESET_PIN, OUTPUT);
  digitalWrite(BLE_RESET_PIN, HIGH);
#endif


#ifdef FTDI_DIRECT_CONNECT
  // do some auto setup of surrounding pins so 
  // we can just plug in an FTDI-USB thingy in line with the
  // ardweeny/sippino/whatever on a breadboard.
  #if (SOFTWARE_SERIAL_TX_PIN != (SOFTWARE_SERIAL_RX_PIN + 1))
    #error "If using FTDI_DIRECT_CONNECT TX must be the pin after RX!"
  #endif
  pinMode(SOFTWARE_SERIAL_TX_PIN + 1, INPUT); // DTR, ignore it
  // TX
  // RX
  pinMode(SOFTWARE_SERIAL_RX_PIN - 1, INPUT); // VCC, ignore it
  pinMode(SOFTWARE_SERIAL_RX_PIN - 2, OUTPUT); // CTS
  pinMode(SOFTWARE_SERIAL_RX_PIN - 3, OUTPUT); // GND

  digitalWrite(SOFTWARE_SERIAL_RX_PIN - 2, LOW); // set CTS low
  digitalWrite(SOFTWARE_SERIAL_RX_PIN - 3, LOW); // set GND low
#endif

  // set the baud rate for serialUI interface
  (SUISerialPortPointer)->begin(SERIALUI_PORT_BAUD);
  // set the data rate for the SoftwareSerial port
  (BLESerialPortPointer)->begin(BLE_UART_BAUD);

  delay(1000); // give everyone some breathing time

  context.currentState = StateStandby;
  context.connection_id = 99; // random "bad" value



  (SUISerialPortPointer)->println(' ');
  mySUI.println(F("BLE112 tester bootup"));


  // set up internal status handlers
  // (these are technically optional)
  ble112.onBusy = onBusy;
  ble112.onIdle = onIdle;
  ble112.onTimeout = onTimeout;


#ifdef USE_HARDWARE_WAKEUP_BEFORE_TX
  // initialize BLE wake-up pin to allow (not force) sleep mode
  pinMode(BLE_WAKEUP_PIN, OUTPUT);
  digitalWrite(BLE_WAKEUP_PIN, LOW);
  // ONLY enable these if you are using the <wakeup_pin> parameter in your firmware's hardware.xml file
  ble112.onBeforeTXCommand = onBeforeTXCommand;
  ble112.onTXCommandComplete = onTXCommandComplete;

#endif

  // set up BGLib response handlers (called almost immediately after sending commands)
  // (these are also technicaly optional)
  ble112.ble_rsp_system_hello = my_rsp_system_hello;
  ble112.ble_rsp_gap_set_scan_parameters = my_rsp_gap_set_scan_parameters;
  ble112.ble_rsp_gap_discover = my_rsp_gap_discover;
  ble112.ble_rsp_gap_end_procedure = my_rsp_gap_end_procedure;




  // event handlers, get called when stuff happens asynchronously
  ble112.ble_evt_connection_status = my_ble_evt_connection_status;

  ble112.ble_evt_attclient_group_found = my_ble_evt_attclient_group_found;

  ble112.ble_evt_attclient_find_information_found = my_ble_evt_attclient_find_information_found;

  ble112.ble_evt_attclient_procedure_completed = my_ble_evt_attclient_procedure_completed;

  ble112.ble_evt_attclient_attribute_value = my_ble_evt_attclient_attribute_value;

  ble112.ble_evt_gap_scan_response = my_evt_gap_scan_response;

  setupMenus(); // setup the serialUI menus
}



void loop() {

  // this is a regular SerialUI loop(), which just
  // checks for a serial user and handles their 
  // requests.  Peppered throughout, 
  // a few checkActivity()s just 'cause it doesn't hurt.

  if (mySUI.checkForUser(150))
  {

    WAIT_AND_CHECK_BLE(5);

    // we have a user initiating contact, show the
    // greeting message and prompt
    mySUI.enter();


    /* Now we keep handling the serial user's
    ** requests until they exit or timeout.
    */
    while (mySUI.userPresent())
    {
      // actually respond to requests, using

      mySUI.handleRequests();


      WAIT_AND_CHECK_BLE(10);
    }


  } /* end if we had a user on the serial line */

  WAIT_AND_CHECK_BLE(25);


}



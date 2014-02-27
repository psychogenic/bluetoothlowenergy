
#ifndef BLE_CLIENT_TEST_DEFS
#define BLE_CLIENT_TEST_DEFS


typedef void (*standardUUIDInfoHandler)(const struct ble_msg_attclient_find_information_found_evt_t *msg);
standardUUIDInfoHandler hasHandler(const uint8array &  uuidArray);


#endif

#ifndef ESPEASY_CUSTOM_H
#define ESPEASY_CUSTOM_H

// Minimal ESPEasy build used by CI to check that P250 compiles (and to provide a test firmware).

#define CONTROLLER_SET_NONE
#define NOTIFIER_SET_NONE
#define PLUGIN_SET_NONE

#define USES_P001   // Switch input
#define USES_P008   // Wiegand reader, to test P250 in loopback
#define USES_P026   // SysInfo
#define USES_P033   // Dummy device
#define USES_P250   // Output - Wiegand (emulator)

#define USES_C005   // Home Assistant (openHAB) MQTT
#define USES_C008   // Generic HTTP

#endif // ESPEASY_CUSTOM_H

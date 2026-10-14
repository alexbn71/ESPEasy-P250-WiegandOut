#include "_Plugin_Helper.h"
#ifdef USES_P250

// #######################################################################################################
// ################################# Plugin 250: Wiegand Output (emulator) ###############################
// #######################################################################################################

/*
   Emulates a Wiegand reader/keypad: sends codes on two GPIOs (D0, D1) to a device with a Wiegand input,
   e.g. the reader input of an alarm panel.

   Commands:
     WiegandOut,<code>               send <code> with the configured frame (bits + optional parity)
     WiegandOut,<facility>,<card>    as above, code = facility << 16 | card (26 bit: FC 0..255, card 0..65535)
     WiegandOutRaw,<bits>,<value>    send <bits> bits of <value> as is (no parity added)
     WiegandOutKeys,<keys>           send keypad keys one by one: 0-9, * (10), # (11), A-F
   Numbers can be decimal, 0x.. (hex) or 0b.. (binary).

   The value 'Code' holds the last frame sent (parity bits included), and is sent to the controllers
   and the rules (event <taskname>#Code) once all queued frames are sent.

   History:
   2026-10-14 Don't resend the last code at every task interval
   2026-10-12 First version
 */

# define PLUGIN_250
# define PLUGIN_ID_250         250
# define PLUGIN_NAME_250       "Output - Wiegand (emulator)"
# define PLUGIN_VALUENAME1_250 "Code"

# include "src/PluginStructs/P250_data_struct.h"

boolean Plugin_250(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      auto& dev = Device[++deviceCount];
      dev.Number         = PLUGIN_ID_250;
      dev.Type           = DEVICE_TYPE_DUAL;
      dev.VType          = Sensor_VType::SENSOR_TYPE_ULONG;
      dev.ValueCount     = 1;
      dev.SendDataOption = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_250);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_250));
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
      event->String1 = formatGpioName_output(F("D0 (Green)"));
      event->String2 = formatGpioName_output(F("D1 (White)"));
      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      P250_FRAME_BITS  = P250_DEF_FRAME_BITS;
      P250_PARITY      = 1;
      P250_PULSE_US    = P250_DEF_PULSE_US;
      P250_INTERVAL_US = P250_DEF_INTERVAL_US;
      P250_KEY_MODE    = static_cast<int>(P250_KeyMode::Bits4);
      P250_KEY_GAP_MS  = P250_DEF_KEY_GAP_MS;
      P250_OUTPUT_MODE = static_cast<int>(P250_OutputMode::OpenDrain);
      success          = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      {
        const __FlashStringHelper *options[] = {
          F("Open drain (idle: open, pulse: 0V) - needs a pull-up on the panel side"),
          F("Push-pull (idle: 3.3V, pulse: 0V)"),
          F("Inverted (idle: 0V, pulse: 3.3V) - for a transistor pulling the line low"),
        };
        const int optionValues[] = {
          static_cast<int>(P250_OutputMode::OpenDrain),
          static_cast<int>(P250_OutputMode::PushPull),
          static_cast<int>(P250_OutputMode::ActiveHigh),
        };
        const FormSelectorOptions selector(NR_ELEMENTS(optionValues), options, optionValues);
        selector.addFormSelector(F("Output mode"), F("omode"), P250_OUTPUT_MODE);
        addFormNote(F("The ESP32 is NOT 5V tolerant: with a 5V/12V Wiegand line use a transistor or a level shifter."));
      }

      addFormSubHeader(F("Frame (WiegandOut)"));
      addFormNumericBox(F("Frame length"), F("bits"), P250_FRAME_BITS, 4, 64);
      addUnit(F("bit"));
      addFormCheckBox(F("Add parity bits"), F("parity"), P250_PARITY != 0);
      addFormNote(F("Standard 26 bit: even parity + 8 bit facility + 16 bit card + odd parity. "
                    "34 bit: even parity + 32 bit + odd parity."));

      addFormSubHeader(F("Keypad (WiegandOutKeys)"));
      {
        const __FlashStringHelper *options[] = {
          F("4 bit per key"),
          F("8 bit per key (high nibble = inverted key)"),
        };
        const int optionValues[] = {
          static_cast<int>(P250_KeyMode::Bits4),
          static_cast<int>(P250_KeyMode::Bits8),
        };
        const FormSelectorOptions selector(NR_ELEMENTS(optionValues), options, optionValues);
        selector.addFormSelector(F("Key format"), F("kmode"), P250_KEY_MODE);
      }
      addFormNumericBox(F("Pause between keys"), F("kgap"), P250_KEY_GAP_MS, 1, 5000);
      addUnit(F("ms"));

      addFormSubHeader(F("Timing"));
      addFormNumericBox(F("Pulse width"), F("pulse"), P250_PULSE_US, 10, 5000);
      addUnit(F("µs"));
      addFormNumericBox(F("Bit interval"), F("interval"), P250_INTERVAL_US, 20, 20000);
      addUnit(F("µs"));
      addFormNote(F("Typical: pulse 50µs, interval 1000-2000µs."));

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      P250_OUTPUT_MODE = getFormItemInt(F("omode"));
      P250_FRAME_BITS  = getFormItemInt(F("bits"));
      P250_PARITY      = isFormItemChecked(F("parity")) ? 1 : 0;
      P250_KEY_MODE    = getFormItemInt(F("kmode"));
      P250_KEY_GAP_MS  = getFormItemInt(F("kgap"));
      P250_PULSE_US    = getFormItemInt(F("pulse"));
      P250_INTERVAL_US = getFormItemInt(F("interval"));
      success          = true;
      break;
    }

    case PLUGIN_INIT:
    {
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P250_data_struct(event));
      P250_data_struct *P250_data = static_cast<P250_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = nullptr != P250_data && P250_data->plugin_init(event);
      break;
    }

    case PLUGIN_READ:
    {
      // Nothing to read: the value is updated and sent when a frame is sent (sendNext).
      // Returning true here would resend the last code to controllers and rules at every task interval.
      success = false;
      break;
    }

    case PLUGIN_TASKTIMER_IN:
    {
      P250_data_struct *P250_data = static_cast<P250_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = nullptr != P250_data && P250_data->plugin_timer_in(event);
      break;
    }

    case PLUGIN_WRITE:
    {
      P250_data_struct *P250_data = static_cast<P250_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = nullptr != P250_data && P250_data->plugin_write(event, string);
      break;
    }
  }

  return success;
}

#endif // USES_P250

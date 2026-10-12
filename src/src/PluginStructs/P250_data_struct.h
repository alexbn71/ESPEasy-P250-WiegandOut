#ifndef PLUGINSTRUCTS_P250_DATA_STRUCT_H
#define PLUGINSTRUCTS_P250_DATA_STRUCT_H

#include "../../_Plugin_Helper.h"
#ifdef USES_P250

# include <deque>

# include "P250_wiegand.h"

# define P250_FRAME_BITS      PCONFIG(0) // Bits per frame of WiegandOut (4..64)
# define P250_PARITY          PCONFIG(1) // 1 = leading even + trailing odd parity bit
# define P250_PULSE_US        PCONFIG(2) // Pulse width (µs)
# define P250_INTERVAL_US     PCONFIG(3) // Time between the start of two bits (µs)
# define P250_KEY_MODE        PCONFIG(4) // Keypad frame type, see P250_KeyMode
# define P250_KEY_GAP_MS      PCONFIG(5) // Pause between keys / queued frames (ms)
# define P250_OUTPUT_MODE     PCONFIG(6) // Electrical output mode, see P250_OutputMode

# define P250_DEF_FRAME_BITS  26
# define P250_DEF_PULSE_US    50
# define P250_DEF_INTERVAL_US 1000
# define P250_DEF_KEY_GAP_MS  50
# define P250_MAX_QUEUE       64

enum class P250_KeyMode : uint8_t {
  Bits4 = 0, // 1 key = 4 bits (0-9, * = 10, # = 11)
  Bits8 = 1, // 1 key = 8 bits, high nibble is the inverted key
};

enum class P250_OutputMode : uint8_t {
  OpenDrain   = 0, // Idle = high-Z (pull-up on the receiving side), pulse = low
  PushPull    = 1, // Idle = 3.3V, pulse = 0V
  ActiveHigh  = 2, // Idle = 0V, pulse = 3.3V (to drive an NPN/MOSFET pulling the line low)
};

struct P250_frame {
  uint8_t  bits;
  uint64_t value;
};

struct P250_data_struct : public PluginTaskData_base {
public:

  P250_data_struct(struct EventStruct *event);

  P250_data_struct() = delete;
  virtual ~P250_data_struct() = default;

  bool plugin_init(struct EventStruct *event);
  bool plugin_write(struct EventStruct *event,
                    const String      & string);
  bool plugin_timer_in(struct EventStruct *event);

private:

  bool queueFrame(uint8_t  bits,
                  uint64_t value);
  bool queueKeys(const String& keys);
  void sendNext(struct EventStruct *event);
  void sendFrame(const P250_frame& frame);
  void pulse(int8_t pin);
  void setIdle(int8_t pin);

  std::deque<P250_frame> _queue;

  int8_t          _pinD0       = -1;
  int8_t          _pinD1       = -1;
  uint8_t         _frameBits   = P250_DEF_FRAME_BITS;
  bool            _parity      = true;
  uint16_t        _pulseUs     = P250_DEF_PULSE_US;
  uint16_t        _intervalUs  = P250_DEF_INTERVAL_US;
  P250_KeyMode    _keyMode     = P250_KeyMode::Bits4;
  uint16_t        _keyGapMs    = P250_DEF_KEY_GAP_MS;
  P250_OutputMode _outputMode  = P250_OutputMode::OpenDrain;
  bool            _busy        = false;
  bool            _initialised = false;
};

#endif // ifdef USES_P250
#endif // ifndef PLUGINSTRUCTS_P250_DATA_STRUCT_H

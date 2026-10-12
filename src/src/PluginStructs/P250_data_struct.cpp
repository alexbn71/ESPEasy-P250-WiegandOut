#include "../PluginStructs/P250_data_struct.h"

#ifdef USES_P250

P250_data_struct::P250_data_struct(struct EventStruct *event) {
  _pinD0      = CONFIG_PIN1;
  _pinD1      = CONFIG_PIN2;
  _frameBits  = constrain(P250_FRAME_BITS, 4, 64);
  _parity     = P250_PARITY != 0 && _frameBits >= 4;
  _pulseUs    = constrain(P250_PULSE_US, 10, 5000);
  _intervalUs = max(static_cast<int>(_pulseUs) + 10, static_cast<int>(constrain(P250_INTERVAL_US, 20, 20000)));
  _keyMode    = static_cast<P250_KeyMode>(P250_KEY_MODE);
  _keyGapMs   = constrain(P250_KEY_GAP_MS, 1, 5000);
  _outputMode = static_cast<P250_OutputMode>(P250_OUTPUT_MODE);
}

bool P250_data_struct::plugin_init(struct EventStruct *event) {
  if (!validGpio(_pinD0) || !validGpio(_pinD1) || (_pinD0 == _pinD1)) {
    addLog(LOG_LEVEL_ERROR, F("WiegandOut: D0 and D1 must be two different GPIOs"));
    return false;
  }
  setIdle(_pinD0);
  setIdle(_pinD1);
  _initialised = true;
  return true;
}

void P250_data_struct::setIdle(int8_t pin) {
  switch (_outputMode) {
    case P250_OutputMode::OpenDrain:
      digitalWrite(pin, HIGH);
      pinMode(pin, OUTPUT_OPEN_DRAIN);
      digitalWrite(pin, HIGH);
      break;
    case P250_OutputMode::PushPull:
      digitalWrite(pin, HIGH);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      break;
    case P250_OutputMode::ActiveHigh:
      digitalWrite(pin, LOW);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      break;
  }
}

void P250_data_struct::pulse(int8_t pin) {
  const bool active = _outputMode == P250_OutputMode::ActiveHigh ? HIGH : LOW;

  digitalWrite(pin, active);
  delayMicroseconds(_pulseUs);
  digitalWrite(pin, !active);
}

bool P250_data_struct::queueFrame(uint8_t bits, uint64_t value) {
  if ((bits == 0) || (bits > 64) || (_queue.size() >= P250_MAX_QUEUE)) {
    return false;
  }
  _queue.push_back({ bits, value });
  return true;
}

bool P250_data_struct::queueKeys(const String& keys) {
  if (keys.isEmpty() || ((_queue.size() + keys.length()) > P250_MAX_QUEUE)) { return false; }

  for (size_t i = 0; i < keys.length(); ++i) {
    if (P250_wiegand::keyValue(keys[i]) < 0) {
      addLog(LOG_LEVEL_ERROR, concat(F("WiegandOut: invalid key: "), String(keys[i])));
      return false;
    }
  }

  for (size_t i = 0; i < keys.length(); ++i) {
    const uint8_t key = P250_wiegand::keyValue(keys[i]);

    if (_keyMode == P250_KeyMode::Bits8) {
      queueFrame(8, P250_wiegand::keyFrame8(key));
    } else {
      queueFrame(4, key);
    }
  }
  return true;
}

void P250_data_struct::sendFrame(const P250_frame& frame) {
  for (int8_t bit = frame.bits - 1; bit >= 0; --bit) {
    const bool one = (frame.value >> bit) & 1;

    const unsigned long start = micros();
    pulse(one ? _pinD1 : _pinD0);

    if (bit > 0) {
      while ((micros() - start) < _intervalUs) {}
    }
  }
}

void P250_data_struct::sendNext(struct EventStruct *event) {
  if (_queue.empty()) {
    _busy = false;
    return;
  }
  const P250_frame frame = _queue.front();

  _queue.pop_front();
  sendFrame(frame);

  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLogMove(LOG_LEVEL_INFO, strformat(F("WiegandOut: sent %d bits 0x%s"),
                                         frame.bits, ull2String(frame.value, HEX).c_str()));
  }

  UserVar.setSensorTypeLong(event->TaskIndex, frame.value);

  if (_queue.empty()) {
    _busy = false;
    sendData(event);
  } else {
    _busy = true;
    Scheduler.setPluginTaskTimer(_keyGapMs, event->TaskIndex, 0);
  }
}

bool P250_data_struct::plugin_timer_in(struct EventStruct *event) {
  sendNext(event);
  return true;
}

bool P250_data_struct::plugin_write(struct EventStruct *event, const String& string) {
  if (!_initialised) { return false; }

  const String cmd = parseString(string, 1);

  if (!cmd.startsWith(F("wiegandout"))) { return false; }

  bool queued = false;

  if (equals(cmd, F("wiegandout"))) {
    // WiegandOut,<code>  or  WiegandOut,<facility>,<card>
    uint64_t a = 0, b = 0;
    const String arg1 = parseString(string, 2);
    const String arg2 = parseString(string, 3);

    if (!validUInt64FromString(arg1, a)) { return false; }

    if (!arg2.isEmpty()) {
      if (!validUInt64FromString(arg2, b)) { return false; }
      a = (a << 16) | (b & 0xFFFF); // Card number is always the lowest 16 data bits
    }
    const uint64_t frame = _parity ? P250_wiegand::addParity(a, _frameBits)
                                   : (_frameBits >= 64 ? a : (a & ((1ull << _frameBits) - 1)));
    queued = queueFrame(_frameBits, frame);
  } else if (equals(cmd, F("wiegandoutraw"))) {
    // WiegandOutRaw,<bits>,<value>  (sent as is, no parity added)
    uint32_t bits  = 0;
    uint64_t value = 0;

    if (!validUIntFromString(parseString(string, 2), bits) ||
        !validUInt64FromString(parseString(string, 3), value) ||
        (bits == 0) || (bits > 64)) {
      return false;
    }

    if (bits < 64) { value &= (1ull << bits) - 1; }
    queued = queueFrame(bits, value);
  } else if (equals(cmd, F("wiegandoutkeys"))) {
    // WiegandOutKeys,<keys>  e.g. WiegandOutKeys,1234#
    queued = queueKeys(parseStringKeepCase(string, 2));
  } else {
    return false;
  }

  if (!queued) {
    addLog(LOG_LEVEL_ERROR, F("WiegandOut: invalid command or queue full"));
    return true; // Command recognised, don't report 'unknown command'
  }

  if (!_busy) {
    sendNext(event);
  }
  return true;
}

#endif // ifdef USES_P250

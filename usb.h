#ifndef USB_H_
#define USB_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "FreeRTOS.h"
#include "base.h"
#include "config.h"
#include "semphr.h"
#include "utils.h"

status USBInit();

status StartUSBTask();

class USBOutputAddIn {
 public:
  USBOutputAddIn();

  virtual void SetIdle(uint8_t idle_rate);
  virtual void SetBoot(bool is_boot_protocol);

 protected:
  SemaphoreHandle_t semaphore_;
  uint8_t idle_rate_;
  bool is_boot_protocol_;
};

class USBKeyboardOutput : public KeyboardOutputDevice, public USBOutputAddIn {
 public:
  static std::shared_ptr<USBKeyboardOutput> GetUSBKeyboardOutput();

  void OutputTick() override;
  void SetConfigMode(bool is_config_mode) override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  void SendKeycode(uint8_t keycode) override;
  void SendKeycode(const std::vector<uint8_t>& keycode) override;
  void SendConsumerKeycode(uint16_t keycode) override;
  void ChangeActiveLayers(const std::vector<bool>&) override {}

 protected:
  USBKeyboardOutput();

  std::array<std::array<uint8_t, 8 + 256 / 8>, 2> double_buffer_;
  uint8_t active_buffer_;
  uint8_t boot_protocol_kc_count_;
  uint16_t consumer_keycode_;
  bool is_config_mode_;
  bool has_key_output_;
};

class USBMouseOutput : public MouseOutputDevice, public USBOutputAddIn {
 public:
  static std::shared_ptr<USBMouseOutput> GetUSBMouseOutput();

  void OutputTick() override;
  void SetConfigMode(bool is_config_mode) override;

  void StartOfInputTick() override;
  void FinalizeInputTickOutput() override;

  void MouseKeycode(uint8_t keycode) override;
  void MouseMovement(int8_t x, int8_t y) override;
  void Pan(int8_t x, int8_t y) override;

 protected:
  USBMouseOutput();

  std::array<std::array<int8_t, 5>, 2> double_buffer_;
  uint8_t active_buffer_;
  bool is_config_mode_;
};

enum InterfaceID {
  ITF_KEYBOARD = 0,
  ITF_MOUSE,
  ITF_CONSUMER,

#if CONFIG_DEBUG_ENABLE_USB_SERIAL
  ITF_CDC_CTRL,
  ITF_CDC_DATA,
#endif /* CONFIG_DEBUG_ENABLE_USB_SERIAL */

  ITF_TOTAL,
};

Status RegisterUSBKeyboardOutput(uint8_t tag);
Status RegisterUSBMouseOutput(uint8_t tag);

#endif /* USB_H_ */
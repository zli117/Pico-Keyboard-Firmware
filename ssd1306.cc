#include "ssd1306.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "pico-ssd1306/shapeRenderer/ShapeRenderer.h"
#include "pico-ssd1306/textRenderer/12x16_font.h"
#include "pico-ssd1306/textRenderer/16x32_font.h"
#include "pico-ssd1306/textRenderer/5x8_font.h"
#include "pico-ssd1306/textRenderer/8x8_font.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

// Debug
#include "hardware/timer.h"

using pico_ssd1306::Size;
using pico_ssd1306::SSD1306;
using pico_ssd1306::WriteMode;

SSD1306Display::SSD1306Display(i2c_inst_t* i2c, uint8_t sda_pin,
                               uint8_t scl_pin, uint8_t i2c_addr,
                               NumRows num_rows, bool flip, uint32_t sleep_s)
    : i2c_(i2c),
      sda_pin_(sda_pin),
      scl_pin_(scl_pin),
      i2c_addr_(i2c_addr),
      num_rows_(num_rows),
      num_cols_(128),
      sleep_s_(sleep_s),
      buffer_idx_(0),
      buffer_changed_(false),
      send_buffer_(true),
      last_active_s_(0),
      sleep_(false) {
  i2c_init(i2c_, 400 * 1000);
  gpio_set_function(sda_pin_, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin_, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin_);
  gpio_pull_up(scl_pin_);

  busy_wait_ms(250);

  std::fill(double_buffer_[0].begin(), double_buffer_[0].end(), 0);
  std::fill(double_buffer_[1].begin(), double_buffer_[1].end(), 0);
  display_ = std::make_unique<SSD1306>(
      i2c_, i2c_addr_, num_rows_ == 64 ? Size::W128xH64 : Size::W128xH32);
  display_->setBuffer(double_buffer_[1].data());
  if (flip) {
    display_->setOrientation(0);
  }

  semaphore_ = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphore_);

  busy_wait_ms(250);

  last_active_s_ = time_us_64() / 1000000;
}

void SSD1306Display::OutputTick() {
  // Manually send the buffer to avoid blocking other tasks

  const uint32_t curr_s = time_us_64() / 1000000;

  bool send_buffer = false;
  std::array<uint8_t, FRAMEBUFFER_SIZE + 1> local_copy;
  local_copy[0] = pico_ssd1306::SSD1306_STARTLINE;
  {
    LockSemaphore lock(semaphore_);
    if (send_buffer_) {
      std::copy(double_buffer_[buffer_idx_].begin(),
                double_buffer_[buffer_idx_].end(), local_copy.begin() + 1);
      send_buffer = send_buffer_;
      last_active_s_ = curr_s;
    }
    send_buffer_ = false;
  }

  if (!sleep_ && curr_s - last_active_s_ >= sleep_s_) {
    sleep_ = true;
    CMD(pico_ssd1306::SSD1306_DISPLAY_OFF);
    return;
  }

  if (send_buffer) {
    if (sleep_) {
      sleep_ = false;
      CMD(pico_ssd1306::SSD1306_DISPLAY_ON);
    }
    CMD(pico_ssd1306::SSD1306_PAGEADDR);
    CMD(0x00);
    CMD(0x07);
    CMD(pico_ssd1306::SSD1306_COLUMNADDR);
    CMD(0x00);
    CMD(127);

    i2c_write_blocking(i2c_, i2c_addr_, local_copy.data(), local_copy.size(),
                       false);
  }
}

void SSD1306Display::StartOfInputTick() { buffer_changed_ = false; }

void SSD1306Display::FinalizeInputTickOutput() {
  LockSemaphore lock(semaphore_);
  if (buffer_changed_) {
    const uint8_t buf_idx = (buffer_idx_ + 1) % 2;
    std::copy(double_buffer_[buf_idx].begin(), double_buffer_[buf_idx].end(),
              double_buffer_[buffer_idx_].begin());
    display_->setBuffer(double_buffer_[buffer_idx_].data());
    buffer_idx_ = buf_idx;
    send_buffer_ = true;
  }
}

void SSD1306Display::ChangeActiveLayers(const std::vector<bool>& layers) {
  for (size_t i = 0; i < layers.size() && i < 16; ++i) {
    DrawRect(1, i * 8 + 1, 7, i * 8 + 7, /*fill=*/true,
             layers[i] ? ADD : SUBTRACT);
  }
}

void SSD1306Display::SetPixel(size_t row, size_t col, Mode mode) {
  display_->setPixel(col, row, (WriteMode)mode);
  buffer_changed_ = true;
}

void SSD1306Display::DrawLine(size_t start_row, size_t start_col,
                              size_t end_row, size_t end_col, Mode mode) {
  pico_ssd1306::drawLine(display_.get(), start_col, start_row, end_col, end_row,
                         (WriteMode)mode);
  buffer_changed_ = true;
}

void SSD1306Display::DrawRect(size_t start_row, size_t start_col,
                              size_t end_row, size_t end_col, bool fill,
                              Mode mode) {
  if (fill) {
    pico_ssd1306::fillRect(display_.get(), start_col, start_row, end_col,
                           end_row, (WriteMode)mode);
  } else {
    pico_ssd1306::drawRect(display_.get(), start_col, start_row, end_col,
                           end_row, (WriteMode)mode);
  }
  buffer_changed_ = true;
}

class BuiltinWrapper : public ScreenOutputDevice::CustomFont {
 public:
  BuiltinWrapper(const uint8_t* buffer) : buffer_(buffer) {}
  const uint8_t* GetFont() const override { return buffer_; }

 private:
  const uint8_t* const buffer_;
};

void SSD1306Display::DrawText(size_t row, size_t col, const std::string& text,
                              Font font, Mode mode) {
  switch (font) {
    case F5X8: {
      DrawText(row, col, text, BuiltinWrapper(font_5x8), mode);
      break;
    }
    case F8X8: {
      DrawText(row, col, text, BuiltinWrapper(font_8x8), mode);
      break;
    }
    case F12X16: {
      DrawText(row, col, text, BuiltinWrapper(font_12x16), mode);
      break;
    }
    case F16X32: {
      DrawText(row, col, text, BuiltinWrapper(font_16x32), mode);
      break;
    }
    default:
      break;
  }
}

void SSD1306Display::DrawText(size_t row, size_t col, const std::string& text,
                              const CustomFont& font, Mode mode) {
  const uint8_t* font_buf = font.GetFont();
  const uint8_t width = font_buf[0];
  size_t col_offset = 0;
  for (char c : text) {
    pico_ssd1306::drawChar(display_.get(), font_buf, c, col + col_offset, row,
                           (WriteMode)mode);
    col_offset += width;
  }
  buffer_changed_ = true;
}

void SSD1306Display::CMD(uint8_t cmd) {
  uint8_t data[2] = {0x00, cmd};
  i2c_write_blocking(i2c_, i2c_addr_, data, 2, false);
}

static std::shared_ptr<SSD1306Display> singleton;

static std::shared_ptr<SSD1306Display> GetSSD1306Display(const Configuration*) {
  if (singleton == NULL) {
    singleton = std::make_shared<SSD1306Display>(
        i2c0, 20, 21, 0x3C, SSD1306Display::R_64, true, 120);
  }
  return singleton;
}

static Status keyboard_out =
    DeviceRegistry::RegisterKeyboardOutputDevice(1, true, GetSSD1306Display);
static Status screen_out =
    DeviceRegistry::RegisterScreenOutputDevice(1, true, GetSSD1306Display);
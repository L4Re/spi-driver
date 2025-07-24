/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *            Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/re/util/object_registry>

namespace Spi_server {

struct Xfer_cfg
{
  l4_uint8_t cs;  ///< Chip select ID
  bool cspol;     ///< Chip select polarity

  unsigned clk;   ///< Clock frequency in Hz or frequency divider
  bool cpol;      ///< Clock polarity
  bool cpha;      ///< Clock phase

  /// TX byte sent to the device during read() as some devices reportedly
  /// require this to be a specific value.
  l4_uint8_t read_tx_val;
};

/**
 * Interface for a spi device to interact with its controller.
 */
struct Controller_if
{
  /* Controller capabilities and features */
  virtual unsigned cs_num() = 0;
  virtual bool cpha_lo_supported() = 0;
  virtual bool cpha_hi_supported() = 0;
  virtual bool cpol_lo_supported() = 0;
  virtual bool cpol_hi_supported() = 0;

  virtual void start_transfer(Xfer_cfg const &cfg) = 0;
  virtual void finish_transfer() = 0;

  virtual long transfer(Xfer_cfg const &cfg, l4_uint8_t const *tx_buf,
                        l4_uint8_t *rx_buf, unsigned len) = 0;
  virtual long write(Xfer_cfg const &cfg, l4_uint8_t const *buf,
                     unsigned len) = 0;
  virtual long read(Xfer_cfg const &cfg, l4_uint8_t *buf, unsigned len) = 0;
  virtual void setup(L4Re::Util::Object_registry *registry) = 0;
};

} // namespace Spi_server

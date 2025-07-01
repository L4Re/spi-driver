/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "spi_device_if.h"

static inline
long
spi_transfer(L4::Cap<Spi_device_ops> dev, unsigned len, l4_uint8_t *tx_buf,
         l4_uint8_t *rx_buf)
{
  L4::Ipc::Array<l4_uint8_t const> wbuf{len, tx_buf};
  L4::Ipc::Array<l4_uint8_t> rbuf{len, rx_buf};

  return dev->transfer(wbuf, rbuf);
}

static inline
long
spi_write_read(L4::Cap<Spi_device_ops> dev, unsigned wlen, l4_uint8_t *tx_buf,
               unsigned rlen, l4_uint8_t *rx_buf)
{
  L4::Ipc::Array<l4_uint8_t const> wbuf{wlen, tx_buf};
  L4::Ipc::Array<l4_uint8_t> rbuf{rlen, rx_buf};

  return dev->write_read(wbuf, rlen, rbuf);
}

static inline
long
spi_write(L4::Cap<Spi_device_ops> dev, unsigned len, l4_uint8_t *tx_buf)
{
  L4::Ipc::Array<l4_uint8_t const> wbuf{len, tx_buf};

  return dev->write(wbuf);
}

static inline
long
spi_read(L4::Cap<Spi_device_ops> dev, unsigned len, l4_uint8_t *rx_buf)
{
  L4::Ipc::Array<l4_uint8_t> rbuf{len, rx_buf};

  return dev->read(len, rbuf);
}


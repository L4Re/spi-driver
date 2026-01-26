/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *            Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/kobject>
#include <l4/sys/cxx/ipc_array>
#include <l4/sys/cxx/ipc_epiface>

struct Spi_device_ops : L4::Kobject_t<Spi_device_ops, L4::Kobject, L4::PROTO_ANY>
{
  struct Xcfg
  {
    bool cspol;   ///< Chip select polarity
    unsigned clk; ///< Clock frequency in Hz
    bool cpol;    ///< Clock polarity
    bool cpha;    ///< Clock phase

    /// TX byte sent to the device during read() as some devices reportedly
    /// require this to be a specific value.
    l4_uint8_t read_tx_val;
  };

  /**
   * Transfer data to and from the currently selected device.
   *
   * \param cfg   Transfer config for target device.
   * \param wbuf  Data to transfer to the device.
   * \param rbuf  Buffer for data to read from the device.
   *
   * \return  L4_EOK or error code.
   */
  L4_INLINE_RPC(long, transfer,
                (Xcfg const cfg,
                 L4::Ipc::Array<l4_uint8_t const> wbuf,
                 L4::Ipc::Array<l4_uint8_t> &rbuf));

  /**
   * Write data to a device and then read from the same device `len` bytes.
   *
   * \param cfg   Transfer config for target device.
   * \param data  Data buffer to write to the device.
   * \param len   Amount of data to read from the device.
   * \param buf   Buffer for the read data.
   *
   * \return  L4_EOK or error code.
   */
  L4_INLINE_RPC(long, write_read,
                (Xcfg const cfg,
                 L4::Ipc::Array<l4_uint8_t const> data, unsigned char len,
                 L4::Ipc::Array<l4_uint8_t> &buf));

  /**
   * Write data to the currently selected device.
   *
   * \param cfg  Transfer config for target device.
   * \param buf  Data to write to the device.
   *
   * \return  L4_EOK or error code.
   */
  L4_INLINE_RPC(long, write,
                (Xcfg const cfg, L4::Ipc::Array<l4_uint8_t const> buf));

  /**
   * Read `len` number of bytes from the currently selected device.
   *
   * \param cfg  Transfer config for target device.
   * \param len  Number of bytes to read.
   * \param buf  Buffer to read the data into.
   *
   * \return  L4_EOK or error code.
   */
  L4_INLINE_RPC(long, read,
                (Xcfg const cfg, unsigned char len,
                 L4::Ipc::Array<l4_uint8_t> &buf));

  typedef L4::Typeid::Rpcs<transfer_t, write_read_t, write_t, read_t> Rpcs;
};

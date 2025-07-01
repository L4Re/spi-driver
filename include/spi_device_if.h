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
  L4_INLINE_RPC(long, transfer,
                (L4::Ipc::Array<l4_uint8_t const> wbuf,
                 L4::Ipc::Array<l4_uint8_t> &rbuf));

  L4_INLINE_RPC(long, write_read,
                (L4::Ipc::Array<l4_uint8_t const> data, unsigned char len,
                 L4::Ipc::Array<l4_uint8_t> &buf));

  L4_INLINE_RPC(long, write, (L4::Ipc::Array<l4_uint8_t const> buf));

  L4_INLINE_RPC(long, read,
                (unsigned char len, L4::Ipc::Array<l4_uint8_t> &buf));

  typedef L4::Typeid::Rpcs<transfer_t, write_read_t, write_t, read_t> Rpcs;
};

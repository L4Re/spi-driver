/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *            Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/factory>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/l4virtio/server/virtio-spi-device>

#include <l4/spi-driver/spi_device_if.h>

#include <l4/spi-driver/server/spi_server.h>
#include <l4/spi-driver/server/spi_controller_if.h>

#include <pthread-l4.h>
#include <memory>
#include <tuple>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <string>
#include <cassert>

#include "debug.h"

namespace Spi_server {

L4Re::Util::Err err(L4Re::Util::Err::Normal, "SPI Server");

static bool
parse_uint_optstring(char const *optstring, unsigned int *out)
{
  char *endp;

  errno = 0;
  unsigned long num = strtoul(optstring, &endp, 16);

  // check that long can be converted to int
  if (errno || *endp != '\0' || num > UINT_MAX)
    return false;

  *out = num;

  return true;
}

static bool
parse_uint_param(L4::Ipc::Varg const &param, char const *prefix,
                 unsigned int *out)
{
  l4_size_t headlen = strlen(prefix);

  if (param.length() < headlen)
    return false;

  char const *pstr = param.value<char const *>();

  if (strncmp(pstr, prefix, headlen) != 0)
    return false;

  std::string tail(pstr + headlen, param.length() - headlen);

  if (!parse_uint_optstring(tail.c_str(), out))
    {
      err.printf("Bad paramter '%s'. Invalid number specified.\n", prefix);
      throw L4::Runtime_error(-L4_EINVAL);
    }

  return true;
}

class Spi_device : public L4::Epiface_t<Spi_device, Spi_device_ops>
{
public:
  Spi_device(Xfer_cfg &cfg, Controller_if *ctrl)
  : _ctrl(ctrl), _cfg(cfg)
  {
    assert(_ctrl);
  }
  long op_transfer(Spi_device_ops::Rights,
                     L4::Ipc::Array_ref<l4_uint8_t const> wbuf,
                     L4::Ipc::Array_ref<l4_uint8_t> &rbuf);
  long op_write_read(Spi_device_ops::Rights,
                     L4::Ipc::Array_ref<l4_uint8_t const> wbuf,
                     unsigned char len, L4::Ipc::Array_ref<l4_uint8_t> &rbuf);
  long op_write(Spi_device_ops::Rights,
                L4::Ipc::Array_ref<l4_uint8_t const> buf);
  long op_read(Spi_device_ops::Rights, unsigned char len,
               L4::Ipc::Array_ref<l4_uint8_t> &buf);

  bool match(l4_uint8_t cs) const { return cs == _cfg.cs; }

private:
  Controller_if *_ctrl;
  Xfer_cfg _cfg;
};

long
Spi_device::op_read(Spi_device_ops::Rights, unsigned char len,
                    L4::Ipc::Array_ref<l4_uint8_t> &buf)
{
  std::vector<l4_uint8_t> buffer(len);
  _ctrl->start_transfer(_cfg, true);
  long err = _ctrl->transfer(_cfg, nullptr, &buffer.front(), len);
  _ctrl->finish_transfer(_cfg, true);

  if (err < 0)
    return err;

  memcpy(buf.data, buffer.data(), len);
  return L4_EOK;
}

long
Spi_device::op_write(Spi_device_ops::Rights,
                     L4::Ipc::Array_ref<l4_uint8_t const> buf)
{
  std::vector<l4_uint8_t> buffer(buf.data, buf.data + buf.length);

  _ctrl->start_transfer(_cfg, true);
  long err = _ctrl->transfer(_cfg, &buffer.front(), nullptr, buf.length);
  _ctrl->finish_transfer(_cfg, true);
  return err;
}

long
Spi_device::op_transfer(Spi_device_ops::Rights,
                          L4::Ipc::Array_ref<l4_uint8_t const> wbuf,
                          L4::Ipc::Array_ref<l4_uint8_t> &rbuf)
{
  unsigned char len = wbuf.length;
  std::vector<l4_uint8_t> wbuffer(wbuf.data, wbuf.data + wbuf.length);
  std::vector<l4_uint8_t> rbuffer(len);
  _ctrl->start_transfer(_cfg, true);
  long err = _ctrl->transfer(_cfg, &wbuffer.front(), &rbuffer.front(), len);
  _ctrl->finish_transfer(_cfg, true);

  if (err < 0)
    return err;

  memcpy(rbuf.data, rbuffer.data(), len);
  return L4_EOK;
}

long
Spi_device::op_write_read(Spi_device_ops::Rights,
                          L4::Ipc::Array_ref<l4_uint8_t const> wbuf,
                          unsigned char len,
                          L4::Ipc::Array_ref<l4_uint8_t> &rbuf)
{
  std::vector<l4_uint8_t> wbuffer(wbuf.data, wbuf.data + wbuf.length);

  _ctrl->start_transfer(_cfg, true);
  long err = _ctrl->transfer(_cfg, &wbuffer.front(), nullptr, wbuf.length);
  if (err < 0)
    {
      _ctrl->finish_transfer(_cfg, true);
      return err;
    }

  std::vector<l4_uint8_t> rbuffer(len);
  err = _ctrl->transfer(_cfg, nullptr, &rbuffer.front(), len);
  _ctrl->finish_transfer(_cfg, true);

  if (err < 0)
    return err;

  memcpy(rbuf.data, rbuffer.data(), len);
  return L4_EOK;
}


class Spi_virtio_request_handler
{
public:
  Spi_virtio_request_handler(Controller_if *ctrl, l4_uint8_t cs,
                             l4_uint8_t read_tx_val = 0)
  : _ctrl(ctrl), _cs(cs), _read_tx_val(read_tx_val)
  {}

  ~Spi_virtio_request_handler() = default;


  L4virtio::Svr::Spi_transfer_result
  handle_transfer(L4virtio::Svr::Spi_transfer_head const &head,
                  l4_uint8_t const *tx_buf, l4_uint8_t *rx_buf, unsigned len)
  {
    if (head.chip_select_id != _cs)
      return L4virtio::Svr::Spi_param_err;

    Xfer_cfg cfg;
    transfer_head2xfer_cfg(head, cfg);
    _ctrl->start_transfer(cfg, false);
    long err = _ctrl->transfer(cfg, tx_buf, rx_buf, len);
    _ctrl->finish_transfer(cfg, false);
    if (err)
      warn().printf("spi-virtio-req::transfer: %li\n", err);

    return (err == L4_EOK) ? L4virtio::Svr::Spi_trans_ok
                           : L4virtio::Svr::Spi_trans_err;
  }

  bool match(l4_uint8_t cs) const { return cs == _cs; }

  unsigned char cs_max_number() const
  { return (unsigned char) _ctrl->cs_num(); }

  unsigned mode_func_supported() const
  {
    enum : unsigned
    {
      Cpha_mode_shift = 0,
      Cpha_mode_lo = 1,
      Cpha_mode_hi = 2,
      Cpol_mode_shift = 2,
      Cpol_mode_lo = 1,
      Cpol_mode_hi = 2
    };

    unsigned mode = 0;

    if (_ctrl->cpha_lo_supported())
      mode |= Cpha_mode_lo << Cpha_mode_shift;
    if (_ctrl->cpha_hi_supported())
      mode |= Cpha_mode_hi << Cpha_mode_shift;

    if (_ctrl->cpol_lo_supported())
      mode |= Cpol_mode_lo << Cpol_mode_shift;
    if (_ctrl->cpol_hi_supported())
      mode |= Cpol_mode_hi << Cpol_mode_shift;

    return mode;
  }

private:
  void transfer_head2xfer_cfg(L4virtio::Svr::Spi_transfer_head const &head,
                              Xfer_cfg &cfg)
  {
    enum
    {
      Mode_cpha = 1,
      Mode_cpol = 2,
      Mode_cspol = 4,
    };

    cfg.cs = head.chip_select_id;
    cfg.cspol = head.mode & Mode_cspol;
    cfg.clk = 512; //head.freq; // XXX
    cfg.cpol = head.mode & Mode_cpol;
    cfg.cpha = head.mode & Mode_cpha;
    cfg.read_tx_val = _read_tx_val;
    cfg.last = head.reserved[0]; // XXX: non-standard!!!
  }

  static Dbg warn() { return Dbg(Dbg::Warn, "ReqHdlr"); }
  static Dbg trace() { return Dbg(Dbg::Trace, "ReqHdlr"); }

  Controller_if *_ctrl;
  l4_uint8_t _cs;
  l4_uint8_t _read_tx_val;
};

class Spi_virtio_device
: public L4virtio::Svr::Virtio_spi<Spi_virtio_request_handler>
{
public:
  Spi_virtio_device(
    Spi_virtio_request_handler *req_hdlr,
    L4Re::Util::Object_registry *registry)
  : Virtio_spi(req_hdlr, registry)
  {}
};

class Spi_factory : public L4::Epiface_t<Spi_factory, L4::Factory>
{
  enum Device_type
  {
    Type_virtio = 0,
    Type_rpc = 1,
  };

public:
  Spi_factory(L4Re::Util::Object_registry *registry, Controller_if *ctrl)
  : _ctrl(ctrl), _registry(registry)
  {}

  /**
   * Establish a connection to a SPI device.
   *
   * \param[out] res   Capability to access device.
   * \param      type  Object protocol: [0, 1]
   * \param      args  Arguments for device creation in order: SPI device chip
   *                   select ID
   */
  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                 l4_umword_t type, L4::Ipc::Varg_list<> &&args);

private:
  static Dbg warn() { return Dbg(Dbg::Warn, "Fab"); }
  static Dbg trace() { return Dbg(Dbg::Trace, "Fab"); }

  bool device_chipselect_free(unsigned cs) const;
  bool device_chipselect_exists(unsigned cs) const;

  Controller_if *_ctrl;
  L4Re::Util::Object_registry *_registry;
  std::vector<std::tuple<std::unique_ptr<Spi_virtio_device>,
                         std::unique_ptr<Spi_virtio_request_handler>>>
    _devices_virtio;
  std::vector<std::unique_ptr<Spi_device>> _devices_rpc;
};

bool
Spi_factory::device_chipselect_free(unsigned cs) const
{
  for (auto const &[dev, hdlr] : _devices_virtio)
      if (hdlr->match(cs))
        return false;

  for (auto const &dev : _devices_rpc)
    if (dev->match(cs))
      return false;

  return true;
}

bool
Spi_factory::device_chipselect_exists(unsigned cs) const
{
  return cs < _ctrl->cs_num();
}

long
Spi_factory::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                       l4_umword_t type, L4::Ipc::Varg_list<> &&args)
{
  warn().printf("Received create request for type %lu\n", type);
  if (type > 1)
    return -L4_ENODEV;

  unsigned dev_cs = 0;
  unsigned dev_cspol = 0;
  unsigned dev_clk = 0;
  unsigned dev_cpol = 0;
  unsigned dev_cpha = 0;
  unsigned dev_read_tx_val = 0;
  for (L4::Ipc::Varg const &arg : args)
    {
      if (!arg.is_of<char const *>())
        {
          err.printf("Unexpected type for argument\n");
          return -L4_EINVAL;
        }

      try
        {
          if (parse_uint_param(arg, "cs=", &dev_cs))
            {
              if (dev_cs >= (1 << 8))
                {
                  err.printf("Requested device chip select %u exceeds 8-bit "
                             "limit\n", dev_cs);
                  return -L4_EINVAL;
                }
              continue;
            }
          if (parse_uint_param(arg, "cspol=", &dev_cspol))
            {
              if (dev_cspol >= 2)
                {
                  err.printf(
                    "Requested device chip select polarity %u is out of range\n",
                    dev_cspol);
                  return -L4_EINVAL;
                }
            }
          if (parse_uint_param(arg, "clk=", &dev_clk))
            {
              // all values accepted
            }
          if (parse_uint_param(arg, "cpol=", &dev_cpol))
            {
              if (dev_cpol >= 2)
                {
                  err.printf(
                    "Requested device clock polarity %u is out of range\n",
                    dev_cpol);
                  return -L4_EINVAL;
                }
            }
          if (parse_uint_param(arg, "cpha=", &dev_cpha))
            {
              if (dev_cpha >= 2)
                {
                  err.printf(
                    "Requested device clock phase %u is out of range\n",
                    dev_cpha);
                  return -L4_EINVAL;
                }
            }
          if (parse_uint_param(arg, "read_tx_val=", &dev_read_tx_val))
            {
              if (dev_read_tx_val >= 256)
                {
                  err.printf(
                    "Requested device TX value for read %u is out of range\n",
                    dev_read_tx_val);
                  return -L4_EINVAL;
                }
            }
        }
      catch (L4::Runtime_error &e)
        {
          return e.err_no();
        }
    }

  if (!device_chipselect_exists(dev_cs))
    return -L4_ENODEV;

  if (!device_chipselect_free(dev_cs))
    return -L4_EEXIST;

  L4::Cap<void> device_ep;

  switch (type)
    {
    case Type_virtio:
      {
        std::unique_ptr<Spi_virtio_request_handler> rqh =
          std::make_unique<Spi_virtio_request_handler>(_ctrl, dev_cs,
                                                       dev_read_tx_val);
        if (!rqh)
          return -L4_EINVAL;

        std::unique_ptr<Spi_virtio_device> spi_dev =
          std::make_unique<Spi_virtio_device>(rqh.get(), _registry);
        if (!spi_dev)
          return -L4_EINVAL;

        device_ep = _registry->register_obj(spi_dev.get());
        if (!device_ep)
          return -L4_EINVAL;

        trace().printf("Created device for chip select 0x%x: %p\n", dev_cs,
                       spi_dev.get());

        _devices_virtio.push_back(
          std::make_tuple(std::move(spi_dev), std::move(rqh)));
        break;
      }
    case Type_rpc:
      {
        Xfer_cfg cfg = {.cs = (l4_uint8_t)dev_cs,
                        .cspol = (bool)dev_cspol,
                        .clk = dev_clk,
                        .cpol = (bool)dev_cpol,
                        .cpha = (bool)dev_cpha,
                        .read_tx_val = (l4_uint8_t)dev_read_tx_val};

        std::unique_ptr<Spi_device> spi_dev =
          std::make_unique<Spi_device>(cfg, _ctrl);

        if (!spi_dev)
          return -L4_EINVAL;

        device_ep = _registry->register_obj(spi_dev.get());
        if (!device_ep)
          return -L4_EINVAL;

        trace().printf("Created device for chip select 0x%x: %p\n", dev_cs,
                       spi_dev.get());

        _devices_rpc.push_back(std::move(spi_dev));
        break;
      }

    default:
      return -L4_ENODEV;
    }

  res = L4::Ipc::make_cap_rw(device_ep);

  return L4_EOK;
}

static std::tuple<L4::Cap<void>, std::unique_ptr<Spi_factory>>
init_factory(L4Re::Util::Object_registry *registry, char const *cap_name,
             Controller_if *ctrl)
{
  Dbg warn(Dbg::Warn, "Fab");
  Dbg trace(Dbg::Trace, "Fab");

  std::unique_ptr<Spi_factory> factory =
    std::make_unique<Spi_factory>(registry, ctrl);

  L4::Cap<void> cap = registry->register_obj(factory.get(), cap_name);

  if (!cap)
    {
      warn.printf("Capability %s for factory interface not in capability table. No SPI devices can be connected.\n",
                   cap_name);
      return std::make_tuple(cap, nullptr);
    }
  else
    trace.printf("factory cap 0x%lx\n", factory->obj_cap().cap());


  return std::make_tuple(cap, std::move(factory));
}

void start_server(Controller_if *ctrl)
{
  using Reg_Server =
    L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks>;

  Dbg warn(Dbg::Warn, "Srv");
  Dbg trace(Dbg::Trace, "Srv");

  // use myself as server thread.
  auto server = std::make_shared<Reg_Server>(Pthread::L4::cap(pthread_self()),
                                             L4Re::Env::env()->factory());

  ctrl->setup(server->registry());

  auto [factory_cap, factory] =
    init_factory(server->registry(), "dev_factory", ctrl);

  if (factory != nullptr)
    {
      trace.printf("factory initialized. start server loop\n");
      // start server loop to allow for factory connections.
      server->loop();

      warn.printf("exited server loop. Cleaning up ...\n");
      server->registry()->unregister_obj(factory.get());
      factory_cap.invalidate();
    }
  else
    warn.printf("Factory registration failed.\n");;
}

} // namespace Spi_server

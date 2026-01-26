/*
 * Copyright (C) 2024-2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *            Jakub Jermar <jakub.jermar@kernkonzept.com>
 *            Martin Küttler <martin.kuettler@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/cxx/bitfield>
#include <l4/util/util.h>
#include <l4/drivers/hw_mmio_register_block>

#include "debug.h"
#include "controller.h"
#include "util.h"

class Ctrl_bcm2835: public Ctrl_base, public L4::Irqep_t<Ctrl_bcm2835>
{
  struct Control
  {
    l4_uint32_t raw;

    explicit Control(Ctrl_bcm2835 *ctrl)
    : raw(ctrl->read32(Mmio_regs::Cs))
    {}

    void update(Ctrl_bcm2835 *ctrl)
    { raw = ctrl->read32(Mmio_regs::Cs); }

    CXX_BITFIELD_MEMBER(23, 23, cspol2, raw);
    CXX_BITFIELD_MEMBER(22, 22, cspol1, raw);
    CXX_BITFIELD_MEMBER(21, 21, cspol0, raw);
    CXX_BITFIELD_MEMBER_RO(20, 20, rxf, raw);
    CXX_BITFIELD_MEMBER_RO(19, 19, rxr, raw);
    CXX_BITFIELD_MEMBER_RO(18, 18, txd, raw);
    CXX_BITFIELD_MEMBER_RO(17, 17, rxd, raw);
    CXX_BITFIELD_MEMBER_RO(16, 16, done, raw);
    CXX_BITFIELD_MEMBER(12, 12, ren, raw);
    CXX_BITFIELD_MEMBER(10, 10, intr, raw);
    CXX_BITFIELD_MEMBER(9, 9, intd, raw);
    CXX_BITFIELD_MEMBER(8, 8, dmaen, raw);
    CXX_BITFIELD_MEMBER(7, 7, ta, raw);
    CXX_BITFIELD_MEMBER(6, 6, cspol, raw);
    CXX_BITFIELD_MEMBER(5, 5, clear_rx, raw);
    CXX_BITFIELD_MEMBER(4, 4, clear_tx, raw);
    CXX_BITFIELD_MEMBER(3, 3, cpol, raw);
    CXX_BITFIELD_MEMBER(2, 2, cpha, raw);
    CXX_BITFIELD_MEMBER(0, 1, cs, raw);
  };

  struct Clock
  {
    l4_uint32_t raw;

    explicit Clock(Ctrl_bcm2835 *ctrl)
    : raw(ctrl->read32(Mmio_regs::Clk))
    {}

    CXX_BITFIELD_MEMBER(0, 15, cdiv, raw);
  };

  // Offsets of the MMIO registers
  enum Mmio_regs
  {
    Cs = 0x0,
    Fifo = 0x4,
    Clk = 0x8,
  };

public:
  Ctrl_bcm2835() = default;

  bool probe(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu) override;
  char const *name() override { return _compatible; }

  void setup(L4Re::Util::Object_registry *registry) override;

  unsigned cs_num() override
  { return 3; } // Note: cs2 is not exposed on GPIO pins on rpi
  bool cpha_lo_supported() override
  { return true; }
  bool cpha_hi_supported() override
  { return true; }
  bool cpol_lo_supported() override
  { return true; }
  bool cpol_hi_supported() override
  { return true; }

  void start_transfer(Spi_server::Xfer_cfg const &cfg) override;
  void finish_transfer(Spi_server::Xfer_cfg const &cfg, bool cs_off) override;

  long transfer(Spi_server::Xfer_cfg const &cfg, l4_uint8_t const *tx_buf,
                l4_uint8_t *rx_buf, unsigned len) override;

  void handle_irq()
  {
    trace().printf("HANDLE IRQ\n");
  }

private:
  static Dbg warn() { return Dbg(Dbg::Warn, "BCM2835"); }
  static Dbg info() { return Dbg(Dbg::Info, "BCM2835"); }
  static Dbg trace() { return Dbg(Dbg::Trace, "BCM2835"); }

  l4_uint32_t read32(l4_uint8_t reg)
  {
//    trace().printf("read reg 0x%x\n", reg);
    return _regs.read<l4_uint32_t>(reg);
  }

  void write32(l4_uint8_t reg, l4_uint32_t val)
  {
//    trace().printf("write 0x%x to reg 0x%x\n", val, reg);
    _regs.write<l4_uint32_t>(val, reg);
  }

  void alloc_ctrl_resources(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu,
                            L4vbus::Device &dev, l4vbus_device_t &devinfo,
                            l4_addr_t &base, l4_addr_t &end,
                            L4::Cap<L4::Irq> &irq, Dbg const &dbg);

  char const *_compatible = "brcm,bcm2835-spi";
  L4drivers::Mmio_register_block<32> _regs;
  l4_addr_t _end;
  L4::Cap<L4::Irq> _irq;
};

void Ctrl_bcm2835::setup(L4Re::Util::Object_registry *registry)
{
  Control c(this);

  c.dmaen() = 0;
  c.intr() = 0;
  c.intd() = 0;
  c.ren() = 0;
  c.ta() = 0;

  write32(Mmio_regs::Cs, c.raw);

  registry->register_obj(this, _irq);
  // TODO communicate this need from the vbus ICU.
  L4Re::chkipc(_irq->unmask(), "Unmask IRQ\n");
}

void Ctrl_bcm2835::start_transfer(Spi_server::Xfer_cfg const &cfg)
{
  Clock clk(this);

  clk.cdiv() = cfg.clk;
  write32(Mmio_regs::Clk, clk.raw);

  Control c(this);

  c.cs() = cfg.cs;

  switch (cfg.cs)
  {
  case 0:
    c.cspol0() = cfg.cspol;
    break;
  case 1:
    c.cspol1() = cfg.cspol;
    break;
  case 2:
    c.cspol2() = cfg.cspol;
    break;
  }

  c.cpol() = cfg.cpol;
  c.cpha() = cfg.cpha;

  c.clear_rx() = 1;
  c.clear_tx() = 1;

  c.ta() = 1;

  write32(Mmio_regs::Cs,  c.raw);
}

void
Ctrl_bcm2835::finish_transfer(Spi_server::Xfer_cfg const & /*cfg*/, bool cs_off)
{
  Control c(this);

  while (!c.done())
    c.update(this);

  if (cs_off)
    {
      c.ta() = 0;
      write32(Mmio_regs::Cs, c.raw);
    }
}

long
Ctrl_bcm2835::transfer(Spi_server::Xfer_cfg const &cfg,
                       l4_uint8_t const *tx_buf, l4_uint8_t *rx_buf,
                       unsigned len)
{
  Control c(this);

  unsigned tx_cnt = 0;
  unsigned rx_cnt = 0;

  while (tx_cnt < len || rx_cnt < len)
    {
      c.update(this);
      while (c.txd() && tx_cnt < len)
        {
          if (tx_buf)
            write32(Mmio_regs::Fifo, (l4_uint32_t) tx_buf[tx_cnt]);
          else
            write32(Mmio_regs::Fifo, (l4_uint32_t) cfg.read_tx_val);
          ++tx_cnt;
          c.update(this);
        }
      while (c.rxd() && rx_cnt < len)
        {
          if (rx_buf)
            rx_buf[rx_cnt] = (l4_uint8_t) read32(Mmio_regs::Fifo);
          else
            (void)read32(Mmio_regs::Fifo);
          ++rx_cnt;
          c.update(this);
        }
    }

  return L4_EOK;
}

void
Ctrl_bcm2835::alloc_ctrl_resources(L4::Cap<L4vbus::Vbus> vbus,
                                   L4::Cap<L4::Icu> icu,
                                   L4vbus::Device &dev,
                                   l4vbus_device_t &devinfo,
                                   l4_addr_t &base, l4_addr_t &end,
                                   L4::Cap<L4::Irq> &irq,
                                   Dbg const &dbg)
{
  base = 0;
  end = 0;
  irq = L4::Cap<L4::Irq>();

  for (unsigned i = 0; i < devinfo.num_resources; ++i)
    {
      l4vbus_resource_t res;

      L4Re::chksys(dev.get_resource(i, &res), "Get vbus device resources");

      switch(res.type)
        {
        case L4VBUS_RESOURCE_MEM:
          {
            alloc_mem_resource(vbus, res, base, end, dbg);
            break;
          }
        case L4VBUS_RESOURCE_IRQ:
          {
            alloc_irq_resource(icu, res, irq, dbg);
            break;
          }
        case L4VBUS_RESOURCE_GPIO:
          {
            enum Gpio_mux
            {
              Fsel0_reg = 0x0,
              Alt_func0 = 0x4, // as defined in bcm2835/bcm2711 spec.
            };

            dbg.printf("Found GPIO resource: [0x%lx, 0x%lx] provider: %li\n",
                       res.start, res.end, res.provider);

            l4_size_t sz = res.end - res.start + 1;
            l4_uint32_t mask = (1 << sz) - 1;

            L4vbus::Gpio_module chipdev(L4vbus::Device(vbus, res.provider));
            L4vbus::Gpio_module::Pin_slice pin_slice(res.start, mask);
            int ret = chipdev.config_pad(pin_slice, Gpio_mux::Fsel0_reg,
                                         Gpio_mux::Alt_func0);
            if (ret != 0)
              dbg.printf("Failed to config GPIO pins: %i. Driver won't work.\n",
                         ret);

            break;
          }
        default:
          dbg.printf("Unhandled resource type %u found: [0x%lx, 0x%lx], "
                     "flags: 0x%x\n",
                     res.type, res.start, res.end, res.flags);
          continue;
        }
    }
}

bool
Ctrl_bcm2835::probe(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu)
{
  L4vbus::Device dev;
  l4vbus_device_t devinfo;

  if (!find_compatible(_compatible, vbus, dev, devinfo))
    return false;

  info().printf("Found a %s device.\n", _compatible);

  l4_addr_t base = 0UL;
  alloc_ctrl_resources(vbus, icu, dev, devinfo, base, _end, _irq, info());
  _regs.set_base(base);

  info().printf("MMIO resource [0x%lx, 0x%lx]; IRQ %s\n", base, _end,
         _irq.is_valid() ? "Yes" : "No");

  return true;
}

/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt philipp.eppelt@kernkonzept.com
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/cxx/bitfield>
#include <l4/drivers/hw_mmio_register_block>

#include "controller.h"
#include "debug.h"
#include "util.h"

#include <thread-l4>
#include <cassert>

namespace Imx8 {

using namespace Spi_server;

class Ctrl_ecspi : public Ctrl_base
{
  enum Reg_offset
  {
    Rxdata = 0,
    Txdata = 4,
    Conreg = 8,
    Configreg = 12,
    Intreg = 16,
    Dmareg = 20,
    Statreg = 24,
    Periodreg = 28,
    Testreg = 32,
    Msgdata = 64,
  };

  l4_uint32_t read_reg(Reg_offset reg)
  { return _regs.read<l4_uint32_t>(reg); }

  void write_reg(Reg_offset reg, l4_uint32_t val)
  { _regs.write<l4_uint32_t>(val, reg); }

  l4_uint32_t read_rxdata(bool print = false)
  {
    l4_uint32_t val = read_reg(Rxdata);
    if (print)
      trace().printf("Read 0x%x from RX\n", val);
    return val;
  }

  void write_txdata(l4_uint32_t val, bool print = false)
  {
    write_reg(Txdata, val);
    if (print)
      trace().printf("Written 0x%x to TX\n", val);
  }

  void write_msgdata(l4_uint32_t val)
  { _regs.write<l4_uint32_t>(val, Msgdata); }

  template <Reg_offset REG, l4_uint32_t RST>
  struct Reg_t
  {
    enum : l4_uint32_t { Reset_val = RST };
    Reg_offset const Offs = REG;
    l4_uint32_t raw = Reset_val;

    void reset() { raw = Reset_val; }
  };

  template <Reg_offset A, l4_uint32_t B>
  void write_reg(Reg_t<A, B> const &reg)
  { write_reg(reg.Offs, reg.raw); }

  template <Reg_offset A, l4_uint32_t B>
  void read_reg(Reg_t<A, B> &reg)
  { reg.raw = read_reg(reg.Offs); }

  struct Con_reg : Reg_t<Conreg, 0UL>
  {
    Con_reg(l4_uint32_t val) { raw = val; }

    CXX_BITFIELD_MEMBER(0, 0, en, raw);
    // CXX_BITFIELD_MEMBER(1, 1, ht, raw); // HT not supported
    CXX_BITFIELD_MEMBER(2, 2, xch, raw);
    CXX_BITFIELD_MEMBER(3, 3, smc, raw);
    CXX_BITFIELD_MEMBER(4, 7, ch_mod, raw);
    CXX_BITFIELD_MEMBER(8, 11, post_div, raw);
    CXX_BITFIELD_MEMBER(12, 15, pre_div, raw);
    CXX_BITFIELD_MEMBER(16, 17, drctl, raw);
    CXX_BITFIELD_MEMBER(18, 19, ch_sel, raw); // only CH 0 supported
    CXX_BITFIELD_MEMBER(20, 31, burst_len, raw);

    void print(char const *prefix = nullptr) const
    {
      info().printf("%s: %sen.%sxch.%ssmc.%smaster.0x%x.0x%x.0x%x.0x%x.0x%x\n",
                    prefix ? prefix : "CON",
                    en().get() ? "+" : "-",
                    xch().get() ? "+" : "-",
                    smc().get() ? "+" : "-",
                    ch_mod().get() & 1 ? "+" : "-",
                    post_div().get(), pre_div().get(),
                    drctl().get(),  ch_sel().get(),
                    burst_len().get());
    }

    void enable()
    {
      en() = 1;
      smc() = 0; // xch bit starts transaction
      // set some slow defaults for the clock frequency, in case we don't get
      // any.
      post_div() = 8;
      pre_div() = 15;
      drctl() = 0;
    }
  };

  struct Config_reg : Reg_t<Configreg, 0UL>
  {
    Config_reg(l4_uint32_t val) { raw = val; }

    enum : unsigned long
    {
      Reserved_mask = 0xe000'0000UL,
    };

    CXX_BITFIELD_MEMBER(0, 3, sclk_pha, raw);
    CXX_BITFIELD_MEMBER(4, 7, sclk_pol, raw);
    CXX_BITFIELD_MEMBER(8, 11, ss_ctl, raw);
    CXX_BITFIELD_MEMBER(12, 15, ss_pol, raw);
    CXX_BITFIELD_MEMBER(16, 19, data_ctl, raw);
    CXX_BITFIELD_MEMBER(20, 23, sclk_ctl, raw);
    // CXX_BITFIELD_MEMBER(24, 28, ht_len, raw);  // HT not supported

    void print(char const *prefix = nullptr) const
    {
      info().printf("%s: ch0: %ssclk_pha.%ssclk_pol.%sss_ctl.%sss_pol.%sdata_ctl.%ssclk_ctl\n",
                    prefix ? prefix : "CONFIG",
                    sclk_pha() & 1 ? "+" : "-",
                    sclk_pol() & 1 ? "+" : "-",
                    ss_ctl() & 1 ? "+" : "-",
                    ss_pol() & 1 ? "+" : "-",
                    data_ctl() & 1 ? "+" : "-",
                    sclk_pha() & 1 ? "+" : "-");
    }
  };

  struct Int_reg : Reg_t<Intreg, 0UL>
  {
    Int_reg(l4_uint32_t val) { raw = val; }

    enum : unsigned long
    {
      Reserved_mask = 0xffff'ff00UL,
    };

    CXX_BITFIELD_MEMBER(0, 0, teen, raw);
    CXX_BITFIELD_MEMBER(1, 1, tren, raw);
    CXX_BITFIELD_MEMBER(2, 2, tfen, raw);
    CXX_BITFIELD_MEMBER(3, 3, rren, raw);
    CXX_BITFIELD_MEMBER(4, 4, rdren, raw);
    CXX_BITFIELD_MEMBER(5, 5, rfen, raw);
    CXX_BITFIELD_MEMBER(6, 6, roen, raw);
    CXX_BITFIELD_MEMBER(7, 7, tcen, raw);

    void print(char const *prefix = nullptr) const
    {
      info().printf("%s: %cteen.%ctren.%ctfen.%crren.%crdren.%crfen.%croen.%ctcen\n",
                    prefix ? prefix : "INT",
                    teen() ? '+' : '-', tren() ? '+' : '-',
                    tfen() ? '+' : '-',
                    rren() ? '+' : '-', rdren() ? '+' : '-',
                    rfen() ? '+' : '-', roen() ? '+' : '-',
                    tcen() ? '+' : '-');
    }
  };

  struct Dma_reg
  {
    enum : unsigned long
    {
      Reserved_mask = 0x4040'ff40UL,
      Reset_val = 0UL
    };

    l4_uint32_t raw = Reset_val;

    CXX_BITFIELD_MEMBER(0, 5, tx_thresh, raw);
    CXX_BITFIELD_MEMBER(7, 7, teden, raw);
    CXX_BITFIELD_MEMBER(16, 21, rx_thresh, raw);
    CXX_BITFIELD_MEMBER(23, 23, rxden, raw);
    CXX_BITFIELD_MEMBER(24, 29, rx_dma_len, raw);
    CXX_BITFIELD_MEMBER(31, 31, rxtden, raw);
  };

  struct Stat_reg : Reg_t<Statreg, 0x3UL>
  {
    Stat_reg(l4_uint32_t val) { raw = val; }

    enum : unsigned long
    {
      Reserved_mask = 0xffff'ff00UL,
      Write_mask = 0x0000'00c0UL,
    };

    CXX_BITFIELD_MEMBER(0, 0, te, raw);
    CXX_BITFIELD_MEMBER(1, 1, tdr, raw);
    CXX_BITFIELD_MEMBER(2, 2, tf, raw);
    CXX_BITFIELD_MEMBER(3, 3, rr, raw);
    CXX_BITFIELD_MEMBER(4, 4, rdr, raw);
    CXX_BITFIELD_MEMBER(5, 5, rf, raw);
    CXX_BITFIELD_MEMBER(6, 6, ro, raw); // write1 clears
    CXX_BITFIELD_MEMBER(7, 7, tc, raw); // write1 clears

    void print(char const *prefix = nullptr) const
    {
      info().printf("%s: %cte.%ctdr.%ctf.%crr.%crdr.%crf.%cro.%ctc\n",
                    prefix ? prefix : "STAT",
                    te() ? '+' : '-', tdr() ? '+' : '-',
                    tf() ? '+' : '-',
                    rr() ? '+' : '-', rdr() ? '+' : '-',
                    rf() ? '+' : '-', ro() ? '+' : '-',
                    tc() ? '+' : '-');
    }
  };

  struct Period_reg : Reg_t<Periodreg, 0UL>
  {
    Period_reg(l4_uint32_t val) { raw = val; }
    enum : unsigned long
    {
      Reserved_mask = 0xffc0'0000UL,
    };

    CXX_BITFIELD_MEMBER(0, 14, sample_period, raw);
    CXX_BITFIELD_MEMBER(15, 15, csrc, raw);
    CXX_BITFIELD_MEMBER(16, 21, csd_ctl, raw);
  };

  struct Test_reg
  {
    enum : unsigned long
    {
      Reserved_mask = 0x7fff'8080UL,
      Reset_val = 0UL
    };

    l4_uint32_t raw = Reset_val;

    CXX_BITFIELD_MEMBER(0, 6, txcnt, raw);
    CXX_BITFIELD_MEMBER(8, 14, rxcnt, raw);
    CXX_BITFIELD_MEMBER(31, 31, lbc, raw);
  };

public:
  Ctrl_ecspi() = default;

  // Ctrl_base functions
  bool probe(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu) override;
  char const *name() override { return _compatible; }

  // Controller_if functions
  // TODO could be configured with multiple GPIOs
  unsigned cs_num() override { return 1; }
  bool cpha_lo_supported() override { return true; }
  bool cpha_hi_supported() override { return true; }
  bool cpol_lo_supported() override { return true; }
  bool cpol_hi_supported() override { return true; }

  void start_transfer(Xfer_cfg const &cfg) override;
  void finish_transfer(Xfer_cfg const &cfg, bool cs_finished) override;

  long transfer(Xfer_cfg const &cfg, l4_uint8_t const *tx_buf,
                l4_uint8_t *rx_buf, unsigned len) override;

  void setup(L4Re::Util::Object_registry *registry) override;

private:
  static Dbg warn() { return Dbg(Dbg::Warn, "ECSPI"); }
  static Dbg info() { return Dbg(Dbg::Info, "ECPSI"); }
  static Dbg trace() { return Dbg(Dbg::Trace, "ECPSI"); }

  enum
  {
    Imx8mp_ecspi_assigned_clock_rate = 0x8000'0000, // aka input clock rate
    Imx8mp_ecspi_low_freq_ref_clock = 0x8000,
  };

  /// compute pre and post divider from device frequency
  void compute_divider(l4_uint32_t const dev_freq, Con_reg &con)
  {
    if (dev_freq == 0)
      return;

    // pre div: input / (pre_div_value + 1)    [1-16]
    // post div: pre-div-output / 2^post_div_value [2^(1-15)]
    l4_uint32_t const input_freq = Imx8mp_ecspi_assigned_clock_rate;
    l4_uint32_t pre_div = 0;
    l4_uint32_t post_shift = 0;
    bool inc_pre = true;
    bool inc_post = false;

    // increase divider values until a SPI clock frequency less than the device
    // frequency is reached.
    while (((input_freq / (pre_div + 1)) >> post_shift) > dev_freq)
      {
        if (inc_pre)
          ++pre_div;
        if (inc_post)
          ++post_shift;

        if (pre_div < 16)
          inc_pre = inc_pre ? false : true; // toggle inc_pre
        else
          inc_pre = false;

        inc_post = inc_pre ? false : true; // increment post only if pre isn't
      }

    assert(post_shift < 16);
    assert(pre_div < 16);

    con.post_div() = post_shift;
    con.pre_div() = pre_div;
  }

  void alloc_ctrl_resources(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu,
                            L4vbus::Device const &dev,
                            l4vbus_device_t const &devinfo,
                            l4_addr_t &base, l4_addr_t &end,
                            L4::Cap<L4::Irq> &irq,
                            L4vbus::Device **gpio_dev,
                            L4vbus::Gpio_pin **cspin,
                            Dbg const &dbg);

  long write(l4_uint8_t const *buf, unsigned const len);
  long read(l4_uint8_t *buf, unsigned const len, l4_uint8_t read_tx_val);
  bool cs_active = false;
  void cs_on(l4_uint8_t /*cs*/)
  {
    if (cs_active)
      return;

    cs_active = true;
    _cspin->set(0);
    l4_usleep(2);
  }

  void cs_off(l4_uint8_t /*cs*/)
  {
    if (!cs_active)
      return;

    cs_active = false;
    _cspin->set(1);
  }

  unsigned wfi()
  {
    static l4_timeout_t timeout_100ms =
      l4_timeout(l4_timeout_from_us(100'000), l4_timeout_from_us(100'000));

    unsigned err = l4_ipc_error(_irq->receive(timeout_100ms), l4_utcb());
    if (err)
      {
        long errn = l4_ipc_to_errno(err);
        warn().printf("Error during wait for interrupt: %s (%li)\n",
                      l4sys_errtostr(errn), errn);
      }

    return err;
  }

  char const *_compatible = "fsl,imx8mp-ecspi";
  L4drivers::Mmio_register_block<32> _regs;
  l4_addr_t _end;
  // TODO make unique_cap; needs unbinding at ICU on destruction.
  L4::Cap<L4::Irq> _irq;
  std::unique_ptr<L4vbus::Device> _gpio_dev;
  std::unique_ptr<L4vbus::Gpio_pin> _cspin;
};

void
Ctrl_ecspi::setup(L4Re::Util::Object_registry *)
{
  L4Re::chksys(_irq->bind_thread(Pthread::L4::cap(pthread_self()), 0x1d07ca4e),
               "Failed to bind to controller IRQ to thread.");
  L4Re::chkipc(_irq->unmask(), "Failed to unmask controller IRQ.");

  Con_reg con(read_reg(Conreg));
  con.print();
  Config_reg conf(read_reg(Configreg));
  conf.print();
  Int_reg intreg(read_reg(Intreg));
  intreg.print();
  Stat_reg stat(read_reg(Statreg));
  stat.print();

  assert(_cspin.get());
  L4Re::chksys(_cspin->setup(L4VBUS_GPIO_SETUP_OUTPUT, 1),
               "Configure GPIO pin as chip select output.");
}

void
Ctrl_ecspi::start_transfer(Xfer_cfg const &cfg)
{
  info().printf("start transfer\n");

  Con_reg con(read_reg(Conreg));
  con.enable();
  compute_divider(cfg.clk, con);
  write_reg(con);

  con.ch_sel() = cfg.cs;
  assert(cfg.cs == 0);
  con.ch_mod() = 1 << cfg.cs;// assume master mode for selected channel
  con.burst_len() = 7;
  write_reg(con);

  Config_reg conf(read_reg(Configreg));
  conf.ss_pol() = cfg.cspol;
  conf.sclk_pol() = cfg.cpol;
  conf.sclk_pha() = cfg.cpha;
  conf.sclk_ctl() = cfg.cpol; // idle behavior values are inverse to sclk_pol
  conf.data_ctl() = 1;
  conf.ss_ctl() = 1;
  write_reg(conf);
  conf.print();

  cs_on(cfg.cs);
}

void
Ctrl_ecspi::finish_transfer(Xfer_cfg const &cfg, bool cs_finished)
{
  info().printf("finish_transfer : %s\n",
                cs_finished ? "turn cs off" : "keep cs on");

  if (cs_finished)
    cs_off(cfg.cs);

  Int_reg intreg(read_reg(Intreg));
  intreg.reset();
  write_reg(intreg);
}

long
Ctrl_ecspi::write(l4_uint8_t const *buf, unsigned const len)
{
  Stat_reg stat(read_reg(Statreg));
  bool all_data_in_tx = false;
  unsigned i = 0;

  while (!all_data_in_tx)
    {
      for (; i < len && !stat.tf(); ++i)
        {
          write_txdata(buf[i], true);
          read_reg(stat);
        }

      trace().printf("wrote %i of %i bytes\n", i, len);
      all_data_in_tx = i == len;

      Int_reg intreg(read_reg(Intreg));
      intreg.teen() = !all_data_in_tx;
      intreg.tren() = 0;
      intreg.tcen() = 1;
      write_reg(intreg);

      Con_reg con(read_reg(Conreg));
      con.xch() = 1;
      write_reg(con);

      trace().printf("%s: wait for _irq\n", __func__);
      wfi();
      trace().printf(" ... done\n");

      read_reg(stat);
      stat.print();
      if (stat.tc())
        {
          Con_reg con(read_reg(Conreg));
          trace().printf("%s: poll conreg xch\n", __func__);
          while (con.xch())
            read_reg(con);
          trace().printf(" ... done\n");

          for (unsigned j = 0; j < len && stat.rr(); ++j)
            { // drop RX data
              read_rxdata();
              read_reg(stat);
            }

          stat.tc() = 1;
          write_reg(stat);
        }
      // else case: transaction not completed, transmitt more data.
      // Ignore potential read-overflow.
    }

  return 0L;
}

long
Ctrl_ecspi::read(l4_uint8_t *buf, unsigned const len, l4_uint8_t read_tx_val)
{
  Con_reg con(read_reg(Conreg));
  Config_reg conf(read_reg(Configreg));

  Stat_reg stat(read_reg(Statreg));
  assert(!stat.tc());

  Int_reg intreg(read_reg(Intreg));
  intreg.reset();
  intreg.rfen() = 1;
  intreg.tcen() = 1;
  intreg.roen() = 1;
  write_reg(intreg);

  while(stat.rr() != 0)
    {
      l4_uint32_t val = read_rxdata();
      trace().printf("%s: Drop data from RXFIFO 0x%x\n", __func__, val);
      read_reg(stat);
    }

  for (unsigned i = 0; i < len; ++i)
    {
      write_txdata(read_tx_val);
      read_reg(stat);
      if (stat.tf())
        break;
    }

  con.xch() =  1;
  write_reg(con);
  con.print();
  stat.print();
  intreg.print();
  conf.print();

  unsigned i = 0;
  while (!stat.tc())
    {
      trace().printf("%s: wait for _irq\n", __func__);
      wfi();
      trace().printf(" ... done\n");

      read_reg(stat);
      if (!stat.tc() && (stat.rf() || stat.ro()) && stat.rr())
        buf[i++] = read_rxdata() & 0xff;
      else if (!stat.tc())
        {
          warn().printf("Something fishy\n");
          con.print();
          stat.print();
        }
    }
  // stat.tc == 1, stat.ro ==?
  if (stat.ro())
    warn().printf("stat.RO set; data loss\n");

  trace().printf("%s: poll conreg xch\n", __func__);
  while (con.xch())
    read_reg(con);
  trace().printf(" ... done\n");

  for (; i < len && stat.rr() == 1; ++i)
    {
      l4_uint32_t rxval = read_rxdata();
      buf[i] = rxval & 0xff;
      read_reg(stat);
    }

  write_reg(stat); // if tc & ro still set; clear.

  return 0L;
}

long
Ctrl_ecspi::transfer(Xfer_cfg const &cfg, l4_uint8_t const *tx_buf,
                     l4_uint8_t *rx_buf, unsigned len)
{
  if (tx_buf)
    write(tx_buf, len);

  if (rx_buf)
    read(rx_buf, len, cfg.read_tx_val);

  return 0L;
}

void
Ctrl_ecspi::alloc_ctrl_resources(L4::Cap<L4vbus::Vbus> vbus,
                                 L4::Cap<L4::Icu> icu,
                                 L4vbus::Device const &dev,
                                 l4vbus_device_t const &devinfo,
                                 l4_addr_t &base, l4_addr_t &end,
                                 L4::Cap<L4::Irq> &irq,
                                 L4vbus::Device **gpio_dev,
                                 L4vbus::Gpio_pin **cspin,
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
            // TODO support multiple pin
            *gpio_dev = new L4vbus::Device(vbus, res.provider);
            *cspin = new L4vbus::Gpio_pin(**gpio_dev, res.start);
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
Ctrl_ecspi::probe(L4::Cap<L4vbus::Vbus> vbus, L4::Cap<L4::Icu> icu)
{
  L4vbus::Device dev;
  l4vbus_device_t devinfo;

  if (!find_compatible(_compatible, vbus, dev, devinfo))
    return false;

  info().printf("Found a %s device.\n", _compatible);

  l4_addr_t base = 0UL;

  L4vbus::Device *gpio_dev = nullptr;
  L4vbus::Gpio_pin *cspin = nullptr;
  alloc_ctrl_resources(vbus, icu, dev, devinfo, base, _end, _irq, &gpio_dev,
                       &cspin, info());
  _regs.set_base(base);
  _gpio_dev.reset(gpio_dev);
  _cspin.reset(cspin);

  info().printf("MMIO resource [0x%lx, 0x%lx]; IRQ %s\n", base, _end,
         _irq.is_valid() ? "Yes" : "No");

  return true;
}

} // namespace Imx8

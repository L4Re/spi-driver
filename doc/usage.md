# SPI controller driver {#l4re_servers_spi_driver}

 The SPI controller driver provides means to separate access to devices on a
 shared SPI bus to multiple clients.
 The assignment is static and assigned during the system configuration (ned
 script).

 The vBus passed to the SPI controller driver shall only contain a single SPI
 controller.


## Command Line Options

SPI controller driver provides the following command line options:

* `-v`

   Increase the verbosity level by one. The supported verbosity levels are
   Quiet, Warn, Info, and Trace. `-v` can be repeated up to three times. The
   default verbosity level is Warn.

* `-q`

   Silence all output of the SPI controller driver.


## Frontends

 Two frontend interfaces are supported:

 * type 0: virtio-spi-device, and
 * type 1: spi-device.

 The virtio-spi-device interface allows for direct usage as a virtIO device.
 For example with uvmm's virtio-proxy a guest can directly access the
 device(s).

 The spi-device interface allows usage of an SPI device from an L4Re
 SPI driver via the RPC interface.


## Factory interface

 The SPI controller driver offers a factory interface to create one of the
 frontends for a specific SPI device, identified via the SPI device chip select
 ID in decimal number format (`"cs=N"`).
 The capability to the factory interface channel must be named `dev_factory`
 in the capability table of the SPI controller driver.

 Each SPI device chip select ID can only be used once, each subsequent create
 call will return `-L4_EEXIST`.

 The create call accepts the following parameters:

* `"cs=<N>"`

  Use SPI device chip select N.

  Integer value.

* `"cspol=0|1"`

  The polarity of the SPI device chip select. 0 denotes CS active low.
  1 denotes CS active high.

* `"read_tx_val=<BYTE>"` (type 0 only)

  This parameter configures the BYTE value to be written to the SPI device while
  performing a half-duplex read operation. Note that some SPI devices require
  specific values for this. This is only relevant for the virtio-spi-device
  (type 0).

## Usage example for BMP280 sensor device

```lua
   local spi_vbus = ld:new_channel()
   -- Connect `spi_vbus` to the vBus containing one spi controller at `io`.

   local spi_drv_fab = ld:new_channel()

   ld:start(
     {
       log = {"spidrv", "g"},
       caps =
       {
         dev_factory = spi_drv_fab:svr(),
         vbus = spi_vbus,
         jdb = L4.Env.jdb
       }
     }, "rom/spi-driver")

    -- 0: create a type 0 frontend: virtio-spi-device
    -- 1: SPI chip select ID of the BMP280 sensor device connected to CE_1
   local spi_dev = spi_drv_fab:create(0, "cs=1", "cspol=0", "read_tx_val=0")

   -- The spi_dev capability can be passed as named cap to a virtio-proxy
   -- device in uvmm.
```


## Supported hardware SPI controllers

 - BCM2835, BCM2711
 - i.MX8 ECSPI
    - single CS line, only master-mode


## device tree snippet for an virtio-spi-device to use with uvmm

This snippet can be used in a uvmm device tree. The `virtio_spi` device node
contains an `spi` node, describing the bus with the `bmp280@0` node as the only
device on this bus.
The `0` value is the chip select ID of the BMP280 sensor device and `spi_bmp`
is the name of the capability to the spi-driver.

```dts
        virtio_spi@5000 {
            compatible = "virtio,mmio";
            reg = <0x5000 0x200>;
            interrupt-parent = <&gic>;
            interrupts = <0 124 4>;
            l4vmm,vdev = "proxy";
            l4vmm,virtiocap = "spi_bmp";
            status = "okay";

            spi {
              compatible = "virtio,device2d";
              #size-cells = <1>;
              #address-cells = <0>;

              bmp280@0 {
                compatible = "bosch,bmp280";
                reg = <0>; /* chip select */
                spi-max-frequency = <10000000>;
              };
            };

        };
```

## Device driver interface

A device driver must deliver the device configuration on each SPI transfer.
This is necessary, because the operating parameters (e.g. frequency) of a
device might change during run-time.

The `struct Xcfg` informs the SPI controller driver about the necessary
parameter settings. Further explanation for each parameter is given below.

```c
  struct Xcfg
  {
    unsigned clk; ///< Clock frequency in Hz
    bool cpol;    ///< Clock polarity
    bool cpha;    ///< Clock phase

    /// TX byte sent to the device during read() as some devices reportedly
    /// require this to be a specific value.
    l4_uint8_t read_tx_val;
  };
```

* `"clk=<max device frequency>"`

  Maximum frequency the given device supports. Only relevant for the type 1
  spi-device clients as the type 0 virtio-spi-device clients set the clock
  value for each Virtio SPI transfer individually.

* `"cpol=0|1"`

  The polarity of the SPI device clock. 0 denotes the rest state of the clock
  is low. 1 denotes the rest state of the clock is high.

* `"cpha=0|1"`

  The phase of the SPI device clock. 0 denotes the first clock transition
  happens in the middle of data bit. 1 denotes the first clock transition
  happens at the beginning of data bit.

* `"read_tx_val=<BYTE>"`

  This parameter configures the BYTE value to be written to the SPI device while
  performing a half-duplex read operation. Note that some SPI devices require
  specific values for this.

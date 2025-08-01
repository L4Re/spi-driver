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
   local spi_dev = spi_drv_fab:create(0, "cs=1")

   -- The spi_dev capability can be passed as named cap to a virtio-proxy
   -- device in uvmm.
```


## Supported hardware SPI controllers

 - BCM2835, BCM2711


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
              };
            };

        };
```

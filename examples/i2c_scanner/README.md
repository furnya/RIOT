I2C Scanner
============

This application scans the configured I2C bus for connected devices. It does
simply call `i2c_read_byte()` for each of the 128 possible 7-bit I2C addresses.
For any address, to which a device is responding on the bus, the corresponding
address is dumped to STDIO.

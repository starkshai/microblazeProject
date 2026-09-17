# SPI 4-to-3 Wire Bridge for MicroBlaze

This project implements a bridge between a standard 4-wire SPI interface (from the Xilinx AXI Quad SPI IP) and a 3-wire SPI device (half-duplex, bidirectional SDIO). A MicroBlaze soft processor controls the SPI transfers. The protocol uses two control bytes (R/W bit, register address) followed by one data byte. The direction of the SDIO line is automatically switched based on the R/W bit.

## Repository Structure

### FPGA Firmware

- **TOP.v**  
  Top-level Verilog module for the FPGA firmware. It instantiates the AXI Quad SPI IP core, the `spi4to3` bridge, and connects all external ports (SPI, UART, clock, reset, etc.).

- **spi4to3.v**  
  Verilog module that bridges the 4-wire SPI interface (SCK, SS, MOSI, MISO) from the MicroBlaze/AXI Quad SPI to a 3-wire SPI interface (SCK, CS, bidirectional SDIO). It contains a finite state machine that:
  - Sends the first two control bytes with the FPGA driving SDIO.
  - Switches SDIO direction based on the R/W bit (first bit of the first control byte).
  - Receives or sends the third data byte accordingly.

- **system.tcl**  
  Tcl script for building the FPGA firmware in Vivado. It creates the block design, configures the AXI Quad SPI IP, adds the custom `spi4to3` module, and applies pin constraints.

### MicroBlaze Software (SDK)

- **mainv1.c**  
  SDK application **without** UART download. It performs a fixed SPI transfer (e.g., reads a predefined register) and prints the result via UART. Useful for basic hardware verification.

- **mainv2.c**  
  SDK application **with** UART download. It waits for 3 bytes from the UART, uses them as the SPI command frame (CMD1, CMD2, DATA), performs one SPI transfer, and returns the received data via UART. This allows interactive debugging from a PC.
  
- **SpiItrp1.c** 
  SpiItrp1.c is an SDK application for MicroBlaze that acts as an interactive SPI bridge over UART. It waits for a 3-byte command frame (CMD1, CMD2, DATA) from the PC serial terminal, performs one SPI transfer in interrupt mode using the AXI Quad SPI core, and returns the 3 bytes received from the SPI slave back over UART. This makes it easy to debug SPI devices from a PC without rebuilding the hardware.

### PC Host Tools (Python)

- **spi_debug.py**  
  Blocking serial debug tool. It prompts the user for a 3-byte HEX command, sends it to the MicroBlaze, and prints the response. The script blocks on `input()` while waiting for user commands, so asynchronous messages from the MicroBlaze are only read after the user sends a command.

- **spi_debug1.py**  
  Dual-threaded serial debug tool. One thread continuously reads the serial port and prints incoming data, while the main thread handles user input. This allows real-time display of asynchronous messages (e.g., startup banner, heartbeats) without blocking the user interface.

## Getting Started

1. **Build the FPGA firmware**  
   Run `system.tcl` in Vivado to generate the bitstream and export the hardware platform.

2. **Build the MicroBlaze application**  
   In Xilinx SDK (2017.4 or later), create an application project and add either `mainv1.c` or `mainv2.c` depending on your needs.

3. **Program the FPGA**  
   Download the bitstream and ELF file to the target board.

4. **Run the PC debug tool**  
   Install Python 3 and `pyserial`:
   ```bash
   pip install pyserial

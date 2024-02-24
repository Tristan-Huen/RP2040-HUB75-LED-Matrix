# RP2040-HUB75-LED-Matrix-
A Raspberry Pi Pico Program that displays images on a HUB75 LED Matrix. Uses PIO and DMA to output quickly to the screen.

# Introduction
This is a simple program that displays images on a HUB75 LED Matrix. It supports 64x64 1/32 scan panels but can be modified to support other sizes. It makes use of a PIO state machine to output data to the matrix and DMA to continuously send data to the transmit FIFO of the state machine. Images are encoded in a specific way in order to allow this. This implementation is not meant to be the most efficient or fastest and is more of a personal side project of mine. One thing I did explore which I have not seen too many Pico projects use is runtime modification of state machine instructions. Note not everything has been fully documented but it will be slowly added.

## Features
* Supports 24-bit color
* Uses BCM (binary code modulation)
* Utilizes PIO instead of bitbanging with the CPU
* Utilizes DMA to transfer data to PIO

# Technical Overview
The following sections give more detail on the implementation

## Image Encoding
The use of PIO and DMA is pretty simple. Each element in the image array is an encoded value in the form [R1 G1 B1 R2 G2 B2 E D C B A]. The top three bits are for the top of the panel, the next three bits are for the bottom of the panel, and the last five bits are for the address pins. This means that one element represents two pixels; one at the top of the panel (any row $\leq$ 31), and one at the bottom of the panel (any row $>$ 31). This reduces it from $64 \times 64$ elements to 2048 elements. Now the size of the array is 16384 elements since each bit plane has 2048 elements which means $8 \times 2048 = 16384$. This allows for a lot of throughput for the PIO and DMA while sacrificing the ease of drawing things to the screen.

Another thing to mention is that the image array is a little bigger than what it would be for a normal image. This is because each array element is 16 bits and we don't use the top five bits. We can compare the amount of bits each one uses. 

### Normal
$4096 \\, \mathrm{elements} \times \dfrac{32 \\, \mathrm{bits}}{\mathrm{element}} \times \dfrac{1 \\, \mathrm{byte}}{8 \\, \mathrm{bits}} \times \dfrac{1 \\, \mathrm{MB}}{1024^{2} \\, \mathrm{B}} = 0.015625 \\, \mathrm{MB}$

### Encoded
$\dfrac{2048 \\, \mathrm{elements}}{1 \\, \mathrm{bit plane}} \times 8 \\, \mathrm{bit planes}  \times \dfrac{16 \\, \mathrm{bits}}{\mathrm{element}} \times \dfrac{1 \\, \mathrm{byte}}{8 \\, \mathrm{bits}} \times \dfrac{1 \\, \mathrm{MB}}{1024^{2} \\, \mathrm{B}} = 0.03125 \\, \mathrm{MB}$

## PIO and DMA
The PIO and DMA are linked as follows:
* DMA sends 2048 elements from the image array to the PIO TX FIFO and raises an interrupt once it finishes.
* In the interrupt handler we do a runtime modification of a state machine instruction which represents the BCM delay value.
* We clear the interrupt flag and set the read address to the next 2048 elements which is the next bit plane.

This runtime modification is of the instruction `in NULL 32 side 0b010`. The 32 is where we modify the value for the BCM delay. The relationship between the shift value and the resulting output is as follows,

`in NULL SHIFT_VAL side 0b010 = 0x4880 - (32-SHIFT_VAL)`

It should be noted that this relationship doesn't hold for the value 32 since the lower five bits control the shift value which means only values from 0 to 31. A shift value of 0 is pointless so when the lower five bits are all zeroes this represents a shift value of 32 instead. This information can be found in the datasheet in Chapter 3 section 3.4.4.

/*
    This file is part of the BeeVDP engine.
    Copyright (C) 2021 BueniaDev.

    BeeVDP is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BeeVDP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BeeVDP.  If not, see <https://www.gnu.org/licenses/>.
*/

// Buenia's Notes:
// This implementation currently covers the TMS9918A VDP.
// All other variants of the TMS99XXA, as well as V9938 and V9958 implementations,
// are currently unsupported at the moment, but I plan to
// support these varaints in the future.
//
// Note that the term "V9938 syntax" is used in this implementation 
// in order to describe a specific TMS9918A mode
// as it is refered to in the V9938 Technical Data Book.
//
// TODO list:
// Implement Graphics II, multi-color and undocumented modes
// Figure out RGB colors for PAL VDP (i.e. TMS9929A)
// Implement 4K/16K VRAM bank selection
// Implement sprite rendering
// TMS9929A support
// Support for other VDP implementations?

#include "beevdp.h"
using namespace beevdp;
using namespace std;

namespace beevdp
{
    TMS9918A::TMS9918A()
    {

    }

    TMS9918A::~TMS9918A()
    {

    }

    // Increment address register
    void TMS9918A::increment_addr()
    {
	// The address register wraps around to 0
	// when it exceeds 0x3FFF
	if (addr_register == 0x3FFF)
	{
	    addr_register = 0;
	}
	else
	{
	    addr_register += 1;
	}
    }

    // Fetch the color corresponding to the given palette number
    BeeVDPRGB TMS9918A::get_color(int color_val)
    {
	// Mask the palette number to between 0 and 15
	color_val &= 0xF;

	// Return the color corresponding to the palette number
	// (Note: Format of a BeeVDPRGB struct is {red, green, blue})
	switch (color_val)
	{
	    case 0: return {0, 0, 0}; break; // Transparent (but return black color here)
	    case 1: return {0, 0, 0}; break; // Black
	    case 2: return {33, 200, 66}; break; // Medium green
	    case 3: return {94, 200, 120}; break; // Light green
	    case 4: return {84, 85, 237}; break; // Dark blue
	    case 5: return {125, 118, 252}; break; // Light blue
	    case 6: return {212, 82, 77}; break; // Dark red
	    case 7: return {66, 235, 245}; break; // Cyan
	    case 8: return {252, 85, 84}; break; // Medium red
	    case 9: return {255, 121, 120}; break; // Light red
	    case 10: return {212, 193, 84}; break; // Dark yellow
	    case 11: return {230, 206, 128}; break; // Light yellow
	    case 12: return {33, 176, 59}; break; // Dark green
	    case 13: return {201, 91, 186}; break; // Magenta
	    case 14: return {204, 204, 204}; break; // Gray
	    case 15: return {255, 255, 255}; break; // White
	}

	// This shouldn't happen
	return {0, 0, 0};
    }

    // Renders a blank screen
    // (Note: this function is called when the VDP is disabled)
    void TMS9918A::render_disabled()
    {
	render_backdrop();
	update_framebuffer();
    }

    // Render the backdrop
    void TMS9918A::render_backdrop()
    {
	// Fill the screen with the backdrop color
	uint16_t vcount = vcounter;

	for (int xpos = 0; xpos < 256; xpos++)
	{
	    BeeVDPRGB color = get_color(backdrop_color);
	    set_pixel(xpos, vcount, color);
	}
    }

    // Sets an individual pixel at ('xpos', 'ypos') to RGB color of 'color'
    void TMS9918A::set_pixel(int xpos, int ypos, BeeVDPRGB color)
    {
	// Sanity check to avoid possible buffer overflows
	if (!inRange(xpos, 0, getWidth()) || !inRange(ypos, 0, getHeight()))
	{
	    return;
	}

	// Set current render line
	render_line = ypos;
	// Update internal linebuffer
	linebuffer[xpos] = color;
    }

    // Update the framebuffer used to display the screen
    void TMS9918A::update_framebuffer()
    {
	// Sanity check to avoid possible buffer overflows
	if (!inRange(render_line, 0, getHeight()))
	{
	    return;
	}

	// Set y-position
	int ypos = render_line;

	// Copy contents of the linebuffer to the current
	// scanline on the framebuffer
	for (int xpos = 0; xpos < getWidth(); xpos++)
	{
	    uint32_t screen_pos = (xpos + (ypos * getWidth()));
	    framebuffer[screen_pos] = linebuffer[xpos];
	}

	// Clear the linebuffer afterwards
	// to prepare for the next line
	linebuffer.fill({0, 0, 0});
    }

    // Render an individual scanline
    void TMS9918A::render_scanline()
    {
	// If the VDP is disabled, render just the backdrop
	if (!is_vdp_enabled)
	{
	    render_disabled();
	    return;
	}

	// Render the backdrop
	render_backdrop();

	// Render the background contents
	switch (mode_val)
	{
	    // Mode 0 (aka. graphics I mode)
	    case 0: render_graphics1(); break;
	    // Mode 1 (aka. text mode)
	    case 1: render_text1(); break;
	    // Mode 2 (aka. graphics II mode)
	    case 2: render_graphics2(); break;
	    default:
	    {
		cout << "Unrecognized VDP mode of " << dec << int(mode_val) << endl;
		exit(0);
	    }
	    break;
	}

	// Update the framebuffer
	update_framebuffer();
    }

    // Render in mode 0
    // (aka. SCREEN 1 in MSX BASIC, and GRAPHIC 1 in V9938 syntax)
    void TMS9918A::render_graphics1()
    {
	uint16_t vcount = vcounter;
	uint32_t name_base = (pattern_name << 10);
	uint32_t color_base = (color_table << 6);
	uint32_t pattern_base = (pattern_gen << 11);
	uint32_t ypos = ((vcount >> 3) << 5);

	for (int tile_col = 0; tile_col < 32; tile_col++)
	{
	    uint32_t name_addr = (name_base + ypos + tile_col);
	    uint8_t name_byte = vram[name_addr];

	    uint32_t pattern_addr = (pattern_base + (name_byte << 3) + (vcount & 0x7));
	    uint8_t pattern_byte = vram[pattern_addr];

	    uint32_t color_addr = (color_base + (name_byte >> 3));
	    uint8_t color_byte = vram[color_addr];

	    for (int pixel = 0; pixel < 8; pixel++)
	    {
		int xpos = ((tile_col << 3) + pixel);
		int colorline = (7 - pixel);

		int pixel_color = 0;

		if (testbit(pattern_byte, colorline))
		{
		    pixel_color = (color_byte >> 4);
		}
		else
		{
		    pixel_color = (color_byte & 0xF);
		}

		if (pixel_color == 0)
		{
		    pixel_color = backdrop_color;
		}

		BeeVDPRGB color = get_color(pixel_color);

		if (xpos < getWidth())
		{
		    set_pixel(xpos, vcount, color);
		}
	    }
	}
    }

    // Render in mode 1
    // (aka. SCREEN 0 in MSX BASIC, and TEXT 1 in V9938 syntax)
    void TMS9918A::render_text1()
    {
	uint16_t vcount = vcounter;
	uint32_t name_base = (pattern_name << 10);
	uint32_t pattern_base = (pattern_gen << 11);
	uint32_t ypos = ((vcount >> 3) * 40);

	for (int tile_col = 0; tile_col < 40; tile_col++)
	{
	    uint32_t name_addr = (name_base + ypos + tile_col);
	    uint8_t name_byte = vram[name_addr];

	    uint32_t pattern_addr = (pattern_base + (name_byte << 3) + (vcount & 0x7));
	    uint8_t pattern_byte = vram[pattern_addr];

	    for (int pixel = 0; pixel < 6; pixel++)
	    {
		int xpos = ((tile_col * 6) + (8 + pixel));
		int colorline = (7 - pixel);

		int pixel_color = 0;

		if (testbit(pattern_byte, colorline))
		{
		    pixel_color = text_color;
		}
		else
		{
		    pixel_color = backdrop_color;
		}

		if (pixel_color == 0)
		{
		    pixel_color = backdrop_color;
		}

		BeeVDPRGB color = get_color(pixel_color);

		if (xpos < getWidth())
		{
		    set_pixel(xpos, vcount, color);
		}
	    }
	}
    }

    // Render in mode 2
    // (aka. SCREEN 2 in MSX BASIC, and GRAPHIC 2 in V9938 syntax)
    void TMS9918A::render_graphics2()
    {
	// TODO: Implement mode 2
	return;
    }

    // Update current VDP mode
    void TMS9918A::update_mode()
    {
	mode_val = ((m3_bit << 2) | (m2_bit << 1) | m1_bit);
    }

    // Write to a VDP register
    void TMS9918A::write_reg(int reg, uint8_t data)
    {
	// Ignore writes to invalid registers
	// (i.e. not registers 0-7)
	if (reg >= 8)
	{
	    return;
	}

	switch (reg)
	{
	    // Register 0 (m2 bit and external video input bit)
	    case 0:
	    {
		m2_bit = testbit(data, 1);
		update_mode();
	    }
	    break;
	    // Register 1 (m1 and m3 bits, VDP and IRQ enable bits,
	    // and sprite magnification/size bits)
	    case 1:
	    {
		is_vdp_enabled = testbit(data, 6);
		is_irq = testbit(data, 5);
		m1_bit = testbit(data, 4);
		m3_bit = testbit(data, 3);
		update_mode();

		if (is_vblank && is_irq)
		{
		    is_irq_gen = true;
		}
	    }
	    break;
	    // Register 2 (pattern name table address)
	    case 2:
	    {
		pattern_name = (data & 0xF);
	    }
	    break;
	    // Register 3 (color table address)
	    case 3:
	    {
		color_table = data;
	    }
	    break;
	    // Register 4 (pattern generator table address)
	    case 4:
	    {
		pattern_gen = (data & 0x7);
	    }
	    break;
	    // Register 7 (text and backdrop colors)
	    case 7:
	    {
		text_color = (data >> 4);
		backdrop_color = (data & 0xF);
	    }
	    break;
	    default: break;
	}
    }

    // Initialize the VDP
    void TMS9918A::init()
    {
	// Fill VRAM with random data to simulate
	// the real hardware
	srand(time(NULL));
	for (int i = 0; i < 0x4000; i++)
	{
	    vram[i] = (rand() & 0xFF);
	}

	// Clear framebuffer and linebuffer
	framebuffer.fill({0, 0, 0});
	linebuffer.fill({0, 0, 0});
	is_vblank = true;
	cout << "TMS9918A::Initialized" << endl;
    }

    // Power off the VDP
    void TMS9918A::shutdown()
    {
	cout << "TMS9918A::Shutting down..." << endl;
    }

    // Write to TMS9918A control port
    void TMS9918A::writeControl(uint8_t data)
    {
	if (is_second_control_write)
	{
	    // Update command word, address register and code register
	    command_word = ((command_word & 0xFF) | (data << 8));
	    addr_register = (command_word & 0x3FFF);
	    code_register = (command_word >> 14);

	    switch (code_register)
	    {
		// Read VRAM
		case 0:
		{
		    // Update the read buffer...
		    read_buffer = vram[addr_register];
		    // ...and increment the address register
		    increment_addr();
		}
		break;
		// Write VRAM
		case 1: break;
		// Write to VDP register
		case 2:
		case 3:
		{
		    int vdp_reg = ((command_word >> 8) & 0x7);
		    uint8_t vdp_data = (command_word & 0xFF);
		    cout << "Writing value of " << hex << int(vdp_data) << " to VDP register of " << dec << int(vdp_reg) << endl;
		    write_reg(vdp_reg, vdp_data);
		}
		break;
		default: break;
	    }

	    is_second_control_write = false;
	}
	else
	{
	    // Update command word and address register
	    command_word = ((command_word & 0xFF00) | data);
	    addr_register = (command_word & 0x3FFF);
	    is_second_control_write = true;
	}
    }

    // Write to TMS9918A data port
    void TMS9918A::writeData(uint8_t data)
    {
	// Write data to VRAM and read buffer
	vram[addr_register] = data;
	read_buffer = data;
	// Increment address register
	increment_addr();
	// Reset "is_second_byte" flag
	is_second_control_write = false;
	read_buffer = data;
    }

    // Check if an IRQ has been generated
    bool TMS9918A::isInterrupt()
    {
	// Prevent IRQ from being fired off more than once per frame
	bool irq_gen = is_irq_gen;
	is_irq_gen = false;
	return irq_gen;
    }

    // Read from TMS9918A status port
    uint8_t TMS9918A::readStatus()
    {
	// Format of status byte:
	// INT | 5S | C | FS4 | FS3 | FS2 | FS1 | FS0
	uint8_t status_byte = (is_vblank << 7);
	// Reset vblank and "is_second_byte" flags
	is_vblank = false;
	is_second_control_write = false;
	return status_byte;
    }

    // Read from TMS9918A data port
    uint8_t TMS9918A::readData()
    {
	// Reset "is_second_byte" flag
	is_second_control_write = false;
	// Return previous value from read buffer
	uint8_t result = read_buffer;
	// Update the read buffer...
	read_buffer = vram[addr_register];
	// ...and increment the address register
	increment_addr();
	return result;
    }

    // Fetch TMS9918A framebuffer
    // (note: format of BeeVDPRGB struct is {red, green, blue})
    array<BeeVDPRGB, (256 * 192)> TMS9918A::getFramebuffer()
    {
	// The TMS9918A resolution is 256x192
	return framebuffer;
    }

    // Fetch width of TMS9918A framebuffer
    int TMS9918A::getWidth() const
    {
	// The TMS9918A resolution is 256 pixels wide
	return 256;
    }

    // Fetch height of TMS9918A framebuffer
    int TMS9918A::getHeight() const
    {
	// The TMS9918A resolution is 192 pixels high
	return 192;
    }

    // Fetch maximum number of scanlines in TMS9918A
    // (useful for appropriately clocking the VDP)
    int TMS9918A::numScanlines() const
    {
	return 262;
    }

    // Clock the emulated TMS9918A once
    void TMS9918A::chipClock()
    {
	// If the internal vcounter is equal 
	// to the VDP height, we've reached VBlank
	if (vcounter == getHeight())
	{
	    is_vblank = true;

	    // Generate frame IRQ (if enabled)
	    if (is_irq)
	    {
		is_irq_gen = true;
	    }
	}

	// If the internal vcounter is less than the VDP height,
	// render the current scanline
	if (vcounter < getHeight())
	{
	    render_scanline();
	}

	// Increment the internal vcounter
	vcounter += 1;

	// The internal vcounter wraps around to 0
	// when it exceeds the total number of VDP scanlines
	if (vcounter == numScanlines())
	{
	    vcounter = 0;
	}
    }
}
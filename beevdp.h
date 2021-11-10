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

#include <iostream>
#include <cstdint>
#include <array>
#include <random>
#include <ctime>
using namespace std;

namespace beevdp
{
    struct BeeVDPRGB
    {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
    };

    class TMS9918A
    {
	public:
	    TMS9918A();
	    ~TMS9918A();

	    void init();
	    void shutdown();

	    void writeControl(uint8_t data);
	    void writeData(uint8_t data);

	    bool isInterrupt();
	    uint8_t readStatus();
	    uint8_t readData();

	    array<BeeVDPRGB, (256 * 192)> getFramebuffer();

	    int getWidth() const;
	    int getHeight() const;
	    int numScanlines() const;

	    void chipClock();

	private:
	    array<BeeVDPRGB, (256 * 192)> framebuffer;

	    int render_line = 0;
	    array<BeeVDPRGB, 256> linebuffer;

	    bool is_second_control_write = false;
	    uint16_t command_word = 0;
	    uint16_t addr_register = 0;
	    int code_register = 0;

	    uint8_t read_buffer = 0;

	    uint16_t vcounter = 0;

	    bool is_vblank = false;

	    array<uint8_t, 0x4000> vram;

	    void write_reg(int reg, uint8_t data);

	    bool m2_bit = false;
	    bool m1_bit = false;
	    bool m3_bit = false;
	    int mode_val = 0;

	    bool is_vdp_enabled = false;
	    bool is_irq = false;

	    bool is_irq_gen = false;

	    int pattern_name = 0;
	    int color_table = 0;
	    int pattern_gen = 0;

	    int text_color = 0;
	    int backdrop_color = 0;

	    void update_mode();

	    void render_scanline();
	    void render_backdrop();
	    void render_graphics1();
	    void render_text1();
	    void render_graphics2();

	    void render_disabled();

	    void set_pixel(int xpos, int ypos, BeeVDPRGB color);
	    void update_framebuffer();

	    BeeVDPRGB get_color(int color_val);

	    void increment_addr();

	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    template<typename T>
	    bool inRange(T val, T low, T high)
	    {
		return ((val >= low) && (val < high));
	    }
    };
};
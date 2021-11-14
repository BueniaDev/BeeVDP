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
#include <fstream>
#include <cassert>
#include <SDL2/SDL.h>
#include "beevdp.h"
#include "vdpfont.h" // VDP font file as C-array
using namespace beevdp;
using namespace std;

int width = 256;
int height = 192;
int scale = 2;

SDL_Window *window = NULL;
SDL_Renderer *render = NULL;
SDL_Texture *texture = NULL;

int sdl_error(string message)
{
    cout << message << " SDL_Error: " << SDL_GetError() << endl;
    return 1;
}

void shutdown()
{
    if (texture != NULL)
    {
	SDL_DestroyTexture(texture);
	texture = NULL;
    }

    if (render != NULL)
    {
	SDL_DestroyRenderer(render);
	render = NULL;
    }

    if (window != NULL)
    {
	SDL_DestroyWindow(window);
	window = NULL;
    }

    SDL_Quit();
}

void updatevdp(TMS9918A &vdp)
{
    for (int i = 0; i < vdp.numScanlines(); i++)
    {
	vdp.chipClock();

	// Handle interrupts (like we would on a real TMS9918A)
	if (vdp.isInterrupt())
	{
	    // Clear the status register's IRQ flag
	    vdp.readStatus();
	}
    }

    assert(render && texture);
    SDL_UpdateTexture(texture, NULL, vdp.getFramebuffer().data(), (width * sizeof(BeeVDPRGB)));
    SDL_RenderClear(render);
    SDL_RenderCopy(render, texture, NULL, NULL);
    SDL_RenderPresent(render);
}

template<typename T>
bool inRange(T value, T low, T high)
{
    return ((value >= low) && (value < high));
}

struct VDPTuple
{
    bool is_invalid = false;
    uint16_t addr = 0;
    uint8_t data = 0;
};

VDPTuple get_tuple(int xpos, int ypos)
{
    VDPTuple tuple;
    if (!inRange(xpos, 0, 256) || !inRange(ypos, 0, 193))
    {
	cout << "Invalid coordinate of (" << dec << xpos << "," << dec << ypos << ")" << endl;
	tuple.is_invalid = true;
	return tuple;
    }

    uint16_t horiz_byte_offs = ((xpos / 8) * 8);
    uint16_t vert_start_addr = ((ypos / 8) * 256);
    tuple.addr = (horiz_byte_offs + vert_start_addr + (ypos % 8));
    tuple.data = (1 << (7 - (xpos % 8)));
    return tuple;
}

void plot_pixel_m2(TMS9918A &vdp, int xpos, int ypos)
{
    VDPTuple tuple = get_tuple(xpos, ypos);

    if (!tuple.is_invalid)
    {
	vdp.writeControl((tuple.addr & 0xFF));
	vdp.writeControl((tuple.addr >> 8) | 0x40);
	vdp.writeData(tuple.data);
    }
}

void reset_vdp(TMS9918A &vdp)
{
    vdp.writeControl(0x00);
    vdp.writeControl(0x40);

    for (int i = 0; i < 0x4000; i++)
    {
	vdp.writeData(0x00);
    }

    for (int i = 0; i <= 7; i++)
    {
	vdp.writeControl(0x00);
	vdp.writeControl((i | 0x80));
    }
}

void mode0_test(TMS9918A &vdp)
{
    cout << "Launching Graphics I mode..." << endl;
    // 0x0000-0x07FF: Sprite Patterns
    // 0x0800-0x0FFF: Pattern Table
    // 0x1000-0x107F: Sprite Attributes
    // 0x1080-0x13FF: Unused
    // 0x1400-0x17FF: Name Table
    // 0x1800-0x1FFF: Unused
    // 0x2000-0x201F: Color Table
    // 0x2020-0x3FFF: Unused

    vdp.writeControl(0x00);
    vdp.writeControl(0x80);

    vdp.writeControl(0x80);
    vdp.writeControl(0x81);

    vdp.writeControl(0x05);
    vdp.writeControl(0x82);

    vdp.writeControl(0x80);
    vdp.writeControl(0x83);

    vdp.writeControl(0x01);
    vdp.writeControl(0x84);

    vdp.writeControl(0x20);
    vdp.writeControl(0x85);

    vdp.writeControl(0x00);
    vdp.writeControl(0x86);

    vdp.writeControl(0x04);
    vdp.writeControl(0x87);

    // Set VRAM address to pattern table
    vdp.writeControl(0x00);
    vdp.writeControl(0x48);

    // Fill the pattern table with the font data
    for (size_t i = 0; i < vdpfont_len; i++)
    {
	vdp.writeData(vdpfont[i]);
    }

    // Set VRAM address to name table
    vdp.writeControl(0x00);
    vdp.writeControl(0x54);

    // Clear the VRAM data
    // On the real hardware, the VRAM contains random data on startup
    for (int i = 0; i < 768; i++) // 32x24 tiles = 768 bytes
    {
	vdp.writeData(0x00);
    }

    // Set VRAM address to color table
    vdp.writeControl(0x00);
    vdp.writeControl(0x60);

    for (int i = 0; i < 0x1800; i++)
    {
	vdp.writeData(0xF4);
    }

    // Set VDP's internal address register to the name table location + 32 to start on the second line of tiles
    vdp.writeControl(0x20);
    vdp.writeControl(0x54); // 0x14 | 0x40

    string text_str = "Hello, world!";

    for (auto &data : text_str)
    {
	vdp.writeData(data);
    }

    vdp.writeControl(0xC0);
    vdp.writeControl(0x81);
}

void mode1_test(TMS9918A &vdp)
{
    cout << "Launching Text mode..." << endl;
    // 0x0000-0x07FF: Pattern table
    // 0x0800-0x0BBF: Name table
    // 0x0BC0-0x3FFF: Unused

    vdp.writeControl(0x00);
    vdp.writeControl(0x80);

    vdp.writeControl(0x90);
    vdp.writeControl(0x81);

    vdp.writeControl(0x02);
    vdp.writeControl(0x82);

    vdp.writeControl(0x00);
    vdp.writeControl(0x84);

    vdp.writeControl(0x20);
    vdp.writeControl(0x85);

    vdp.writeControl(0x00);
    vdp.writeControl(0x86);

    vdp.writeControl(0xF4);
    vdp.writeControl(0x87);

    // Set VRAM address to pattern table
    vdp.writeControl(0x00);
    vdp.writeControl(0x40);

    // Fill the pattern table with the font data
    for (size_t i = 0; i < vdpfont_len; i++)
    {
	vdp.writeData(vdpfont[i]);
    }

    // Set VRAM address to name table
    vdp.writeControl(0x00);
    vdp.writeControl(0x48);

    // Clear the VRAM data
    // On the real hardware, the VRAM contains random data on startup
    for (int i = 0; i < 768; i++) // 40x24 tiles = 960 bytes
    {
	vdp.writeData(0x00);
    }

    // Set VDP's internal address register to the name table location + 40 to start on the second line of tiles
    vdp.writeControl(0x28);
    vdp.writeControl(0x48); // 0x08 | 0x40

    string text_str = "Hello, world!";

    for (auto &data : text_str)
    {
	vdp.writeData(data);
    }

    vdp.writeControl(0xD0);
    vdp.writeControl(0x81);
}

void mode2_test(TMS9918A &vdp)
{
    cout << "Launching Graphics II mode..." << endl;
    // 0x0000-0x17FF: Pattern table
    // 0x1800-0x1FFF: Sprite patterns
    // 0x2000-0x37FF: Color table
    // 0x3800-0x3AFF: Name table
    // 0x3B00-0x3BFF: Sprite attributes
    // 0x3C00-0x3FFF: Unused

    vdp.writeControl(0x02);
    vdp.writeControl(0x80);

    vdp.writeControl(0x82);
    vdp.writeControl(0x81);

    vdp.writeControl(0x0E);
    vdp.writeControl(0x82);

    vdp.writeControl(0xFF);
    vdp.writeControl(0x83);

    vdp.writeControl(0x03);
    vdp.writeControl(0x84);

    vdp.writeControl(0x76);
    vdp.writeControl(0x85);

    vdp.writeControl(0x03);
    vdp.writeControl(0x86);

    vdp.writeControl(0x04);
    vdp.writeControl(0x87);

    vdp.writeControl(0x00);
    vdp.writeControl(0x60);

    for (int i = 0; i < 0x1800; i++)
    {
	vdp.writeData(0xF4);
    }

    vdp.writeControl(0x00);
    vdp.writeControl(0x78); // 0x38 | 0x40

    for (int i = 0; i < 768; i++)
    {
	vdp.writeData((i & 0xFF));
    }

    plot_pixel_m2(vdp, 128, 96);

    vdp.writeControl(0xC2);
    vdp.writeControl(0x81);
}

void mode3_test(TMS9918A &vdp)
{
    cout << "Launching Multicolor mode..." << endl;
    // 0x0000-0x07FF: Sprite patterns
    // 0x0800-0x0DFF: Pattern table
    // 0x0E00-0x0FFF: Unused
    // 0x1000-0x107F: Sprite attributes
    // 0x1080-0x13FF: Unused
    // 0x1400-0x16FF: Name table
    // 0x1700-0x3FFF: Unused

    vdp.writeControl(0x00);
    vdp.writeControl(0x80);

    vdp.writeControl(0x8B);
    vdp.writeControl(0x81);

    vdp.writeControl(0x05);
    vdp.writeControl(0x82);

    vdp.writeControl(0x01);
    vdp.writeControl(0x84);

    vdp.writeControl(0x20);
    vdp.writeControl(0x85);

    vdp.writeControl(0x00);
    vdp.writeControl(0x86);

    vdp.writeControl(0x04);
    vdp.writeControl(0x87);

    vdp.writeControl(0x00);
    vdp.writeControl(0x54); // 0x14 | 0x40

    for (int i = 0; i < 6; i++)
    {
	uint8_t data_offs = (i << 5);

	for (int j = 0; j < 128; j++)
	{
	    uint8_t data_byte = (data_offs + (j & 0x1F));
	    vdp.writeData(data_byte);
	}
    }

    vdp.writeControl(0x00);
    vdp.writeControl(0x48); // 0x08 | 0x40

    for (int i = 0; i < 0x600; i++)
    {
	vdp.writeData(0x44);
    }

    vdp.writeControl(0x80);
    vdp.writeControl(0x4B); // 0x0B | 0x40

    vdp.writeData(0xF4);

    vdp.writeControl(0xCB);
    vdp.writeControl(0x81);
}

void dump_vram(TMS9918A &vdp)
{
    vdp.writeControl(0x00);
    vdp.writeControl(0x00);

    array<uint8_t, 0x4000> vram_dump;

    for (int i = 0; i < 0x4000; i++)
    {
	vram_dump[i] = vdp.readData();
    }

    time_t currenttime = time(nullptr);
    string filepath = "BeeVDP_vram_dump_";
    filepath.append(to_string(currenttime));
    filepath.append(".bin");
    ofstream file(filepath.c_str(), ios::out | ios::binary);
    file.write((char*)vram_dump.data(), vram_dump.size());
    file.close();
}

int main(int argc, char *argv[])
{
    TMS9918A vdp;
    vdp.init();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
	return sdl_error("SDL2 could not be initialized!");
    }

    window = SDL_CreateWindow("BeeVDP-Tests", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (vdp.getWidth() * scale), (vdp.getHeight() * scale), SDL_WINDOW_SHOWN);

    if (window == NULL)
    {
	shutdown();
	return sdl_error("Window could not be created!");
    }

    render = SDL_CreateRenderer(window, -1, 0);

    if (render == NULL)
    {
	shutdown();
	return sdl_error("Renderer could not be created!");
    }

    texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, vdp.getWidth(), vdp.getHeight());

    if (texture == NULL)
    {
	shutdown();
	return sdl_error("Texture could not be created!");
    }

    SDL_SetRenderDrawColor(render, 0, 0, 0, 255);

    reset_vdp(vdp);

    cout << "Press any of the keys below in order to control the example project." << endl;
    cout << "0: Display example of Graphics I mode" << endl;
    cout << "1: Display example of Text mode" << endl;
    cout << "2: Display example of Graphics II mode" << endl;
    cout << "3: Display example of Multicolor mode" << endl;
    cout << "D: Dump VRAM to file" << endl;
    cout << endl;

    bool quit = false;
    SDL_Event event;

    while (!quit)
    {
	while (SDL_PollEvent(&event))
	{
	    switch (event.type)
	    {
		case SDL_QUIT: quit = true; break;
		case SDL_KEYDOWN:
		{
		    switch (event.key.keysym.sym)
		    {
			case SDLK_0:
			{
			    reset_vdp(vdp);
			    mode0_test(vdp);
			}
			break;
			case SDLK_1:
			{
			    reset_vdp(vdp);
			    mode1_test(vdp);
			}
			break;
			case SDLK_2:
			{
			    reset_vdp(vdp);
			    mode2_test(vdp);
			}
			break;
			case SDLK_3:
			{
			    reset_vdp(vdp);
			    mode3_test(vdp);
			}
			break;
			case SDLK_d:
			{
			    dump_vram(vdp);
			}
			break;
		    }
		}
		break;
	    }
	}

	updatevdp(vdp);
    }

    vdp.shutdown();
    shutdown();
    return 0;
}
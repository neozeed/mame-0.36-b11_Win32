/***************************************************************************

remaining issues:
	busy loop should be patched out

Please contact Phil Stroffolino (phil@maya.com) if there are any questions
regarding this driver.

Tiger Road is (C) 1987 Romstar/Capcom USA

Memory Overview:
	0xfe0800    sprites
	0xfec000    text
	0xfe4000    input ports,dip switches (read); sound out, video reg (write)
	0xfe4002	protection (F1 Dream only)
	0xfe8000    scroll registers
	0xff8200    palette
	0xffC000    working RAM
	0xffEC70    high scores (not saved)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

extern int tigeroad_base_bank;
extern unsigned char *tigeroad_scrollram;

void tigeroad_scrollram_w(int offset,int data);
void tigeroad_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static unsigned char *ram;	/* for hi score save */

/*
 F1 Dream protection code written by Eric Hustvedt (hustvedt@ma.ultranet.com).

 The genuine F1 Dream game uses an 8751 microcontroller as a protection measure.
 Since the microcontroller's ROM is unavailable all interactions with it are handled
 via blackbox algorithm.

 Some notes:
 - The 8751 is triggered via location 0xfe4002, in place of the soundlatch normally
 	present. The main cpu writes 0 to the location when it wants the 8751 to perform some work.
 - The 8751 has memory which shadows locations 0xffffe0-0xffffff of the main cpu's address space.
 - The word at 0xffffe0 contains an 'opcode' which is written just before the write to 0xfe4002.
 - Some of the writes to the soundlatch may not be handled. 0x27fc is the main sound routine, the
 	other locations are less frequently used.
*/

static int f1dream_613ea_lookup[16] = {
0x0052, 0x0031, 0x00a7, 0x0043, 0x0007, 0x008a, 0x00b1, 0x0066, 0x009f, 0x00cc, 0x0009, 0x004d, 0x0033, 0x0028, 0x00d0, 0x0025};

static int f1dream_613eb_lookup[256] = {
0x0001, 0x00b5, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b6, 0x00b7, 0x0001, 0x00b8, 0x002f, 0x002f, 0x002f, 0x002f, 0x00b9,
0x00aa, 0x0031, 0x00ab, 0x00ab, 0x00ab, 0x00ac, 0x00ad, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x0091,
0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x009b, 0x0091,
0x00bc, 0x0092, 0x000b, 0x0009, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0073, 0x0001, 0x0098, 0x0099, 0x009a, 0x009b, 0x0091,
0x00bc, 0x007b, 0x000b, 0x0008, 0x0087, 0x0088, 0x0089, 0x008a, 0x007f, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f, 0x0090, 0x0091,
0x00bd, 0x007b, 0x000b, 0x0007, 0x007c, 0x007d, 0x007e, 0x0001, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086,
0x00bc, 0x0070, 0x000b, 0x0006, 0x0071, 0x0072, 0x0073, 0x0001, 0x0074, 0x000d, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a,
0x00bc, 0x00ba, 0x000a, 0x0005, 0x0065, 0x0066, 0x0067, 0x0068, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
0x00bc, 0x0059, 0x0001, 0x0004, 0x005a, 0x005b, 0x0001, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064,
0x0014, 0x004d, 0x0001, 0x0003, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0001, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058,
0x0014, 0x0043, 0x0001, 0x0002, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x00bb, 0x004a, 0x004b, 0x004c, 0x0001, 0x0001,
0x0014, 0x002b, 0x0001, 0x0038, 0x0039, 0x003a, 0x003b, 0x0031, 0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0001,
0x0014, 0x002d, 0x0001, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0001, 0x0014, 0x0037, 0x0001,
0x0014, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x0001, 0x0001, 0x0001, 0x002a, 0x002b, 0x002c,
0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001e, 0x001e, 0x001e, 0x001f, 0x0020,
0x000c, 0x000d, 0x000e, 0x0001, 0x000f, 0x0010, 0x0011, 0x0012, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x0013 };

static int f1dream_17b74_lookup[128] = {
0x0003, 0x0040, 0x0005, 0x0080, 0x0003, 0x0080, 0x0005, 0x00a0, 0x0003, 0x0040, 0x0005, 0x00c0, 0x0003, 0x0080, 0x0005, 0x00e0,
0x0003, 0x0040, 0x0006, 0x0000, 0x0003, 0x0080, 0x0006, 0x0020, 0x0003, 0x0040, 0x0006, 0x0040, 0x0003, 0x0080, 0x0006, 0x0060,
0x0000, 0x00a0, 0x0009, 0x00e0, 0x0000, 0x00e0, 0x000a, 0x0000, 0x0000, 0x00a0, 0x000a, 0x0020, 0x0000, 0x00e0, 0x000a, 0x0040,
0x0000, 0x00a0, 0x000a, 0x0060, 0x0000, 0x00e0, 0x000a, 0x0080, 0x0000, 0x00a0, 0x000a, 0x00a0, 0x0000, 0x00e0, 0x000a, 0x00c0,
0x0003, 0x0040, 0x0005, 0x0080, 0x0003, 0x0080, 0x0005, 0x00a0, 0x0003, 0x0040, 0x0005, 0x00c0, 0x0003, 0x0080, 0x0005, 0x00e0,
0x0003, 0x0040, 0x0006, 0x0000, 0x0003, 0x0080, 0x0006, 0x0020, 0x0003, 0x0040, 0x0006, 0x0040, 0x0003, 0x0080, 0x0006, 0x0060,
0x0000, 0x00a0, 0x0009, 0x00e0, 0x0000, 0x00e0, 0x000a, 0x0000, 0x0000, 0x00a0, 0x000a, 0x0020, 0x0000, 0x00e0, 0x000a, 0x0040,
0x0000, 0x00a0, 0x000a, 0x0060, 0x0000, 0x00e0, 0x000a, 0x0080, 0x0000, 0x00a0, 0x000a, 0x00a0, 0x0000, 0x00e0, 0x000a, 0x00c0 };

static int f1dream_2450_lookup[32] = {
0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0, 0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0,
0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0, 0x0003, 0x0080, 0x0006, 0x0060, 0x0000, 0x00e0, 0x000a, 0x00c0 };

static void f1dream_protection_w(void)
{
	int indx;
	int value = 255;
	int prevpc = cpu_getpreviouspc();

	if (prevpc == 0x244c)
	{
		/* Called once, when a race is started.*/
		indx = READ_WORD(&ram[0x3ff0]);
		WRITE_WORD(&ram[0x3fe6],f1dream_2450_lookup[indx]);
		WRITE_WORD(&ram[0x3fe8],f1dream_2450_lookup[++indx]);
		WRITE_WORD(&ram[0x3fea],f1dream_2450_lookup[++indx]);
		WRITE_WORD(&ram[0x3fec],f1dream_2450_lookup[++indx]);
	}
	else if (prevpc == 0x613a)
	{
		/* Called for every sprite on-screen.*/
		if ((READ_WORD(&ram[0x3ff6])) < 15)
		{
			indx = f1dream_613ea_lookup[READ_WORD(&ram[0x3ff6])] - (READ_WORD(&ram[0x3ff4]));
			if (indx > 255)
			{
				indx <<= 4;
				indx += READ_WORD(&ram[0x3ff6]) & 0x00ff;
				value = f1dream_613eb_lookup[indx];
			}
		}

		WRITE_WORD(&ram[0x3ff2],value);
	}
	else if (prevpc == 0x17b70)
	{
		/* Called only before a real race, not a time trial.*/
		if ((READ_WORD(&ram[0x3ff0])) >= 0x04) indx = 128;
		else if ((READ_WORD(&ram[0x3ff0])) > 0x02) indx = 96;
		else if ((READ_WORD(&ram[0x3ff0])) == 0x02) indx = 64;
		else if ((READ_WORD(&ram[0x3ff0])) == 0x01) indx = 32;
		else indx = 0;

		indx += READ_WORD(&ram[0x3fee]);
		if (indx < 128)
		{
			WRITE_WORD(&ram[0x3fe6],f1dream_17b74_lookup[indx]);
			WRITE_WORD(&ram[0x3fe8],f1dream_17b74_lookup[++indx]);
			WRITE_WORD(&ram[0x3fea],f1dream_17b74_lookup[++indx]);
			WRITE_WORD(&ram[0x3fec],f1dream_17b74_lookup[++indx]);
		}
		else
		{
			WRITE_WORD(&ram[0x3fe6],0x00ff);
			WRITE_WORD(&ram[0x3fe8],0x00ff);
			WRITE_WORD(&ram[0x3fea],0x00ff);
			WRITE_WORD(&ram[0x3fec],0x00ff);
		}
	}
	else if ((prevpc == 0x27f8) || (prevpc == 0x511a) || (prevpc == 0x5142) || (prevpc == 0x516a))
	{
		/* The main CPU stuffs the byte for the soundlatch into 0xfffffd.*/
		soundlatch_w(2,READ_WORD(&ram[0x3ffc]));
	}
}

static void f1dream_control_w( int offset, int data )
{
	switch( offset )
	{
		case 0:
			tigeroad_base_bank = (data>>8)&0xF;
			break;
		case 2:
			if (errorlog) fprintf(errorlog,"protection write, PC: %04x  FFE1 Value:%01x\n",cpu_get_pc(), ram[0x3fe1]);
			f1dream_protection_w();
			break;
	}
}

static void tigeroad_control_w( int offset, int data )
{
	switch( offset )
	{
		case 0:
			tigeroad_base_bank = (data>>8)&0xF;
			break;
		case 2:
			soundlatch_w(offset,(data >> 8) & 0xff);
			break;
	}
}

static int tigeroad_input_r (int offset);
static int tigeroad_input_r (int offset){
	switch (offset){
		case 0: return (input_port_1_r( offset )<<8) |
						input_port_0_r( offset );

		case 2: return (input_port_3_r( offset )<<8) |
						input_port_2_r( offset );

		case 4: return (input_port_5_r( offset )<<8) |
						input_port_4_r( offset );
	}
	return 0x00;
}

int tigeroad_interrupt(void){
	return 2;
}

void tigeroad_driver_init(void)
{
	install_mem_write_handler(0, 0xfe4000, 0xfe4003, tigeroad_control_w);
}

void f1dream_driver_init(void)
{
	install_mem_write_handler(0, 0xfe4000, 0xfe4003, f1dream_control_w);
}


/***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xfe0800, 0xfe0cff, MRA_BANK1 },
	{ 0xfe0d00, 0xfe1807, MRA_BANK2 },
	{ 0xfe4000, 0xfe4007, tigeroad_input_r },
	{ 0xfec000, 0xfec7ff, MRA_BANK3 },
	{ 0xff8200, 0xff867f, paletteram_word_r },
	{ 0xffc000, 0xffffff, MRA_BANK5 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfe0800, 0xfe0cff, MWA_BANK1, &spriteram, &spriteram_size },
	{ 0xfe0d00, 0xfe1807, MWA_BANK2 },  /* still part of OBJ RAM */
	/*{ 0xfe4000, 0xfe4003, tigeroad_control_w },*/
	{ 0xfec000, 0xfec7ff, MWA_BANK3, &videoram, &videoram_size },
	{ 0xfe8000, 0xfe8003, MWA_BANK4, &tigeroad_scrollram },
	{ 0xfe800c, 0xfe800f, MWA_NOP },    /* fe800e = watchdog or IRQ acknowledge */
	{ 0xff8200, 0xff867f, paletteram_xxxxRRRRGGGGBBBB_word_w, &paletteram },
	{ 0xffc000, 0xffffff, MWA_BANK5, &ram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8000, YM2203_status_port_0_r },
	{ 0xa000, 0xa000, YM2203_status_port_1_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8000, YM2203_control_port_0_w },
	{ 0x8001, 0x8001, YM2203_write_port_0_w },
	{ 0xa000, 0xa000, YM2203_control_port_1_w },
	{ 0xa001, 0xa001, YM2203_write_port_1_w },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x01, IOWP_NOP },   /* ??? */
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( tigeroad )
	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* dipswitch A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* dipswitch B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ))
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "20000 70000 70000" )
	PORT_DIPSETTING(    0x10, "20000 80000 80000" )
	PORT_DIPSETTING(    0x08, "30000 80000 80000" )
	PORT_DIPSETTING(    0x00, "30000 90000 90000" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Very Easy" )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( f1dream )
	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* dipswitch A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* dipswitch B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ))
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x18, 0x18, "F1 Up Point" )
	PORT_DIPSETTING(    0x18, "12" )
	PORT_DIPSETTING(    0x10, "16" )
	PORT_DIPSETTING(    0x08, "18" )
	PORT_DIPSETTING(    0x00, "20" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x40, 0x00, "Version" )
	PORT_DIPSETTING(    0x00, "International" )
	PORT_DIPSETTING(    0x40, "Japan" )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END



static struct GfxLayout text_layout =
{
	8,8,    /* character size */
	2048,   /* number of characters */
	2,      /* bits per pixel */
	{ 4, 0 }, /* plane offsets */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 }, /* x offsets */
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 }, /* y offsets */
	16*8
};

static struct GfxLayout tile_layout =
{
	32,32,  /* tile size */
	2048,   /* number of tiles */
	4,      /* bits per pixel */

	{ 2048*256*8+4, 2048*256*8+0, 4, 0 },

	{ /* x offsets */
		0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
		64*8+0, 64*8+1, 64*8+2, 64*8+3, 64*8+8+0, 64*8+8+1, 64*8+8+2, 64*8+8+3,
		2*64*8+0, 2*64*8+1, 2*64*8+2, 2*64*8+3, 2*64*8+8+0, 2*64*8+8+1, 2*64*8+8+2, 2*64*8+8+3,
		3*64*8+0, 3*64*8+1, 3*64*8+2, 3*64*8+3, 3*64*8+8+0, 3*64*8+8+1, 3*64*8+8+2, 3*64*8+8+3,
	},

	{ /* y offsets */
		0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
		24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16
	},

	256*8
};

static struct GfxLayout sprite_layout =
{
	16,16,  /* tile size */
	4096,   /* number of tiles */
	4,      /* bits per pixel */
	{ 3*4096*32*8, 2*4096*32*8, 1*4096*32*8, 0*4096*32*8 }, /* plane offsets */
	{ /* x offsets */
		0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7
	},
	{ /* y offsets */
		0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8
	},
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x000000, &tile_layout,     0, 16 },   /* colors   0-255 */
	{ 1, 0x100000, &sprite_layout, 256, 16 },   /* colors 256-511 */
	{ 1, 0x180000, &text_layout,   512, 16 },   /* colors 512-575 */
	{ -1 } /* end of array */
};



/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2,          /* 2 chips */
	3579545,    /* 3.579 MHz ? */
	{ YM2203_VOL(25,25), YM2203_VOL(25,25) },
	AY8910_DEFAULT_GAIN,
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};



static struct MachineDriver machine_driver =
{
	{
		{
			CPU_M68000,
			6000000, /* ? Main clock is 24MHz */
			0,
			readmem,writemem,0,0,
			tigeroad_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,    /* 4 Mhz ??? */
			3,  /* memory region #3 */
			sound_readmem,sound_writemem,0,sound_writeport,
			ignore_interrupt,0  /* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2203 */
		}
	},
	60, 2500,   /* frames per second, vblank duration */
				/* vblank duration hand tuned to get proper sprite/background alignment */
	1,  /* CPU slices per frame */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	576, 576,
	0, /* convert color prom routine */

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	0,
	0,
	tigeroad_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( tigeroad )
	ROM_REGION(0x40000) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "tru02.bin",    0x00000, 0x20000, 0x8d283a95 )
	ROM_LOAD_ODD( "tru04.bin",    0x00000, 0x20000, 0x72e2ef20 )

	ROM_REGION_DISPOSE(0x188000) /* temporary space for graphics */
	ROM_LOAD( "tr-01a.bin",   0x000000, 0x20000, 0xa8aa2e59 ) /* tiles */
	ROM_LOAD( "tr-04a.bin",   0x020000, 0x20000, 0x8863a63c )
	ROM_LOAD( "tr-02a.bin",   0x040000, 0x20000, 0x1a2c5f89 )
	ROM_LOAD( "tr05.bin",     0x060000, 0x20000, 0x5bf453b3 )
	ROM_LOAD( "tr-03a.bin",   0x080000, 0x20000, 0x1e0537ea )
	ROM_LOAD( "tr-06a.bin",   0x0A0000, 0x20000, 0xb636c23a )
	ROM_LOAD( "tr-07a.bin",   0x0C0000, 0x20000, 0x5f907d4d )
	ROM_LOAD( "tr08.bin",     0x0E0000, 0x20000, 0xadee35e2 )
	ROM_LOAD( "tr-09a.bin",   0x100000, 0x20000, 0x3d98ad1e ) /* sprites */
	ROM_LOAD( "tr-10a.bin",   0x120000, 0x20000, 0x8f6f03d7 )
	ROM_LOAD( "tr-11a.bin",   0x140000, 0x20000, 0xcd9152e5 )
	ROM_LOAD( "tr-12a.bin",   0x160000, 0x20000, 0x7d8a99d0 )
	ROM_LOAD( "tr01.bin",     0x180000, 0x08000, 0x74a9f08c ) /* 8x8 text */

	ROM_REGION( 0x08000 ) /* tilemap for background */
	ROM_LOAD( "tr13.bin",     0x0000, 0x8000, 0xa79be1eb )

	ROM_REGION( 0x10000 ) /* audio CPU */
	ROM_LOAD( "tru05.bin",    0x0000, 0x8000, 0xf9a7c9bf )
ROM_END

ROM_START( f1dream )
	ROM_REGION(0x40000) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "06j_02.bin",   0x00000, 0x20000, 0x3c2ec697 )
	ROM_LOAD_ODD( "06k_03.bin",   0x00000, 0x20000, 0x85ebad91 )

	ROM_REGION_DISPOSE(0x188000) /* temporary space for graphics */
	ROM_LOAD( "03f_12.bin",   0x000000, 0x10000, 0xbc13e43c ) /* tiles */
	ROM_LOAD( "01f_10.bin",   0x010000, 0x10000, 0xf7617ad9 )
	ROM_LOAD( "03h_14.bin",   0x020000, 0x10000, 0xe33cd438 )
	/* 30000-7ffff empty */
	ROM_LOAD( "02f_11.bin",   0x080000, 0x10000, 0x4aa49cd7 )
	ROM_LOAD( "17f_09.bin",   0x090000, 0x10000, 0xca622155 )
	ROM_LOAD( "02h_13.bin",   0x0a0000, 0x10000, 0x2a63961e )
	/* b0000-fffff empty */
	ROM_LOAD( "03b_06.bin",   0x100000, 0x10000, 0x5e54e391 ) /* sprites */
	/* 110000-11ffff empty */
	ROM_LOAD( "02b_05.bin",   0x120000, 0x10000, 0xcdd119fd )
	/* 130000-13ffff empty */
	ROM_LOAD( "03d_08.bin",   0x140000, 0x10000, 0x811f2e22 )
	/* 150000-15ffff empty */
	ROM_LOAD( "02d_07.bin",   0x160000, 0x10000, 0xaa9a1233 )
	/* 170000-17ffff empty */
	ROM_LOAD( "10d_01.bin",   0x180000, 0x08000, 0x361caf00 ) /* 8x8 text */

	ROM_REGION( 0x08000 ) /* tilemap for background */
	ROM_LOAD( "07l_15.bin",   0x0000, 0x8000, 0x978758b7 )

	ROM_REGION( 0x10000 ) /* audio CPU */
	ROM_LOAD( "12k_04.bin",   0x0000, 0x8000, 0x4b9a7524 )
ROM_END

ROM_START( f1dreamb )
	ROM_REGION(0x40000) /* 256K for 68000 code */
	ROM_LOAD_EVEN( "f1d_04.bin",   0x00000, 0x10000, 0x903febad )
	ROM_LOAD_ODD( "f1d_05.bin",   0x00000, 0x10000, 0x666fa2a7 )
	ROM_LOAD_EVEN( "f1d_02.bin",   0x20000, 0x10000, 0x98973c4c )
	ROM_LOAD_ODD( "f1d_03.bin",   0x20000, 0x10000, 0x3d21c78a )

	ROM_REGION_DISPOSE(0x188000) /* temporary space for graphics */
	ROM_LOAD( "03f_12.bin",   0x000000, 0x10000, 0xbc13e43c ) /* tiles */
	ROM_LOAD( "01f_10.bin",   0x010000, 0x10000, 0xf7617ad9 )
	ROM_LOAD( "03h_14.bin",   0x020000, 0x10000, 0xe33cd438 )
	/* 30000-7ffff empty */
	ROM_LOAD( "02f_11.bin",   0x080000, 0x10000, 0x4aa49cd7 )
	ROM_LOAD( "17f_09.bin",   0x090000, 0x10000, 0xca622155 )
	ROM_LOAD( "02h_13.bin",   0x0a0000, 0x10000, 0x2a63961e )
	/* b0000-fffff empty */
	ROM_LOAD( "03b_06.bin",   0x100000, 0x10000, 0x5e54e391 ) /* sprites */
	/* 110000-11ffff empty */
	ROM_LOAD( "02b_05.bin",   0x120000, 0x10000, 0xcdd119fd )
	/* 130000-13ffff empty */
	ROM_LOAD( "03d_08.bin",   0x140000, 0x10000, 0x811f2e22 )
	/* 150000-15ffff empty */
	ROM_LOAD( "02d_07.bin",   0x160000, 0x10000, 0xaa9a1233 )
	/* 170000-17ffff empty */
	ROM_LOAD( "10d_01.bin",   0x180000, 0x08000, 0x361caf00 ) /* 8x8 text */

	ROM_REGION( 0x08000 ) /* tilemap for background */
	ROM_LOAD( "07l_15.bin",   0x0000, 0x8000, 0x978758b7 )

	ROM_REGION( 0x10000 ) /* audio CPU */
	ROM_LOAD( "12k_04.bin",   0x0000, 0x8000, 0x4b9a7524 )
ROM_END



static int tigeroad_hiload(void)
{
	void *f;


	/* check if the hi score table has already been initialized */
	if (READ_WORD(&ram[0x2c70])== 0x5955 && READ_WORD(&ram[0x2cbe])== 0x5000)
	{
		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread_msbfirst(f,&ram[0x2c70],100);
			ram[0x0092]=ram[0x2cac];
			ram[0x0093]=ram[0x2cad];
			ram[0x0094]=ram[0x2cae];
			ram[0x0095]=ram[0x2caf];
			osd_fclose(f);
		}
		return 1;
	}
	else return 0;  /* we can't load the hi scores yet */
}


static void tigeroad_hisave(void)
{
	void *f;

	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite_msbfirst(f,&ram[0x2c70],100);
		osd_fclose(f);
	}
}


static int f1dream_hiload(void)
{
	void *f;


	/* check if the hi score table has already been initialized */

	if (READ_WORD(&ram[0x312e])== 0x0030 && READ_WORD(&ram[0x3190])== 0x0101)
	{
		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread_msbfirst(f,&ram[0x312e],100); /* high score table */
			osd_fread_msbfirst(f,&ram[0x32e8],100); /* best times */

			osd_fclose(f);

		}
	return 1;
	}
	else return 0;  /* we can't load the hi scores yet */
}


static void f1dream_hisave(void)
{
	void *f;

	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{

		osd_fwrite_msbfirst(f,&ram[0x312e],100); /* high score table */
		osd_fwrite_msbfirst(f,&ram[0x32e8],100); /* best times */
		osd_fclose(f);
	}
}



struct GameDriver driver_tigeroad =
{
	__FILE__,
	0,
	"tigeroad",
	"Tiger Road",
	"1987",
	"Capcom (Romstar license)",
	"Phil Stroffolino (MAME driver)\nTim Lindquist",
	0,
	&machine_driver,
	tigeroad_driver_init,

	rom_tigeroad,
	0,0,0,0,

	input_ports_tigeroad,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	tigeroad_hiload, tigeroad_hisave
};

/* F1 Dream has an Intel 8751 microcontroller for protection */
struct GameDriver driver_f1dream =
{
	__FILE__,
	0,
	"f1dream",
	"F-1 Dream",
	"1988",
	"Capcom (Romstar license)",
	"Paul Leaman\nPhil Stroffolino (MAME driver)\nTim Lindquist\nEric Hustvedt (protection)",
	0,
	&machine_driver,
	f1dream_driver_init,

	rom_f1dream,
	0,0,0,0,

	input_ports_f1dream,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	f1dream_hiload, f1dream_hisave
};

struct GameDriver driver_f1dreamb =
{
	__FILE__,
	&driver_f1dream,
	"f1dreamb",
	"F-1 Dream (bootleg)",
	"1988",
	"bootleg",
	"Paul Leaman\nPhil Stroffolino (MAME driver)\nTim Lindquist",
	0,
	&machine_driver,
	tigeroad_driver_init,

	rom_f1dreamb,
	0,0,0,0,

	input_ports_f1dream,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	f1dream_hiload, f1dream_hisave
};

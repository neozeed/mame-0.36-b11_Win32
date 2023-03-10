/***************************************************************************

Blades of Steel(GX797) (c) 1987 Konami

Preliminary driver by:
	Manuel Abadia <manu@teleline.es>

Interrupts:

	CPU #0 (6309):
	--------------
	* IRQ: not used.
	* FIRQ: generated by VBLANK.
	* NMI: writes the sound command to the 6809 (generated by VBLANK).

	CPU #1 (6809):
	--------------
	* IRQ: triggered by the 6309 when a sound command is written.
	* FIRQ: not used.
	* NMI: not used.

Notes:

	* The game is not working properly because of protection(Konami 051733).
	* Wrong sprite colors.
	* Missing samples.

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"

/* from vidhrdw */
int bladestl_spritebank;
int bladestl_vh_start(void);
void bladestl_vh_stop(void);
void bladestl_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void bladestl_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void bladestl_vreg_w(int offset, int data);

static int bladestl_interrupt( void )
{
	cpu_cause_interrupt(0,HD6309_INT_NMI);
	if (K007342_is_INT_enabled())
        return HD6309_INT_FIRQ;
    else
		return ignore_interrupt();
}

static int trackball_r(int offset)
{
	static int last[4];
	int curr,delta;


	curr = readinputport(5 + offset);
	delta = (curr - last[offset]) & 0xff;
	last[offset] = curr;
	return (delta & 0x80) | (curr >> 1);
}

static void bladestl_bankswitch_w(int offset, int data)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bankaddress;

	/* bits 0 & 1 = coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bits 2 & 3 = lamps */
	osd_led_w(0,(data & 0x04) >> 2);
	osd_led_w(1,(data & 0x08) >> 3);

	/* bit 4 = relay (???) */

	/* bits 5-6 = bank number */
	bankaddress = 0x10000 + ((data & 0x60) >> 5) * 0x2000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* bit 7 = select sprite bank */
	bladestl_spritebank = (data & 0x80) << 3;

}

static void bladestl_sh_irqtrigger_w(int offset, int data)
{
	soundlatch_w(offset, data);
	cpu_cause_interrupt(1,M6809_INT_IRQ);
	if (errorlog) fprintf(errorlog,"(sound) write %02x\n", data);
}

static void bladestl_port_A_w(int offset,int data){
	/* bits 0-4 = uPD7759 sample number (chip 0) */
	UPD7759_message_w( 0, data);
	if (data)
		if (errorlog) fprintf(errorlog,"%04x: (port A) write %02x\n",cpu_get_pc(), data);
}

static void bladestl_port_B_w(int offset,int data){
	/* unknown */
	if (data)
		if (errorlog) fprintf(errorlog,"%04x: (port B) write %02x\n",cpu_get_pc(), data);
}

static void bladestl_speech_ctrl_w(int offset,int data){
	/* not understood yet */
	if (data)
		if (errorlog) fprintf(errorlog,"%04x: (speech_ctrl) write %02x\n",cpu_get_pc(), data);
}

static struct MemoryReadAddress bladestl_readmem[] =
{
	{ 0x0000, 0x1fff, K007342_r },			/* Color RAM + Video RAM */
	{ 0x2000, 0x21ff, K007420_r },			/* Sprite RAM */
	{ 0x2200, 0x23ff, MRA_RAM },			/* ??? */
	{ 0x2400, 0x245f, MRA_RAM },			/* Palette */
	{ 0x2e01, 0x2e01, input_port_3_r },		/* 1P controls */
	{ 0x2e02, 0x2e02, input_port_4_r },		/* 2P controls */
	{ 0x2e03, 0x2e03, input_port_1_r },		/* DISPW #2 */
	{ 0x2e40, 0x2e40, input_port_0_r },		/* DIPSW #1 */
	{ 0x2e00, 0x2e00, input_port_2_r },		/* DIPSW #3, coinsw, startsw */
	{ 0x2f00, 0x2f03, trackball_r },		/* Trackballs */
	{ 0x2f80, 0x2f9f, MRA_RAM },			/* Protection: 051733 */
	{ 0x4000, 0x5fff, MRA_RAM },			/* Work RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },			/* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },			/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bladestl_writemem[] =
{
	{ 0x0000, 0x1fff, K007342_w },				/* Color RAM + Video RAM */
	{ 0x2000, 0x21ff, K007420_w },				/* Sprite RAM */
	{ 0x2200, 0x23ff, MWA_RAM },				/* ??? */
	{ 0x2400, 0x245f, paletteram_xBBBBBGGGGGRRRRR_swap_w, &paletteram },/* palette */
	{ 0x2600, 0x2607, K007342_vreg_w },			/* Video Registers */
	{ 0x2e80, 0x2e80, bladestl_sh_irqtrigger_w },/* cause interrupt on audio CPU */
	{ 0x2ec0, 0x2ec0, watchdog_reset_w },		/* watchdog reset */
	{ 0x2f40, 0x2f40, bladestl_bankswitch_w },	/* bankswitch control */
	{ 0x2f80, 0x2f9f, MWA_RAM },				/* Protection: 051733 */
	{ 0x2fc0, 0x2fc0, MWA_NOP },				/* ??? */
	{ 0x4000, 0x5fff, MWA_RAM },				/* Work RAM */
	{ 0x6000, 0x7fff, MWA_RAM },				/* banked ROM */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress bladestl_readmem_sound[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },				/* RAM */
	{ 0x1000, 0x1000, YM2203_status_port_0_r },	/* YM2203 */
	{ 0x1001, 0x1001, YM2203_read_port_0_r },	/* YM2203 */
	{ 0x4000, 0x4000, UPD7759_busy_r },			/* UPD7759? */
	{ 0x6000, 0x6000, soundlatch_r },			/* soundlatch_r */
	{ 0x8000, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bladestl_writemem_sound[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },				/* RAM */
	{ 0x1000, 0x1000, YM2203_control_port_0_w },/* YM2203 */
	{ 0x1001, 0x1001, YM2203_write_port_0_w },	/* YM2203 */
	{ 0x3000, 0x3000, bladestl_speech_ctrl_w },	/* UPD7759 */
	{ 0x5000, 0x5000, MWA_NOP },				/* ??? */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( bladestl )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x18, "Bonus time set" )
	PORT_DIPSETTING(    0x18, "30 secs" )
	PORT_DIPSETTING(    0x10, "20 secs" )
	PORT_DIPSETTING(    0x08, "15 secs" )
	PORT_DIPSETTING(    0x00, "10 secs" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_DIPNAME( 0x80, 0x80, "Period time set" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x00, "7" )


	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	/* Trackball 1P */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER1, 100, 63, 0x7f, 0, 0)
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1, 100, 63, 0x7f, 0, 0)

	/* Trackball 2P */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 63, 0x7f, 0, 0)
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 63, 0x7f, 0, 0)
INPUT_PORTS_END

INPUT_PORTS_START( bladstle )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x60, "Easy" )
	PORT_DIPSETTING(	0x40, "Normal" )
	PORT_DIPSETTING(	0x20, "Difficult" )
	PORT_DIPSETTING(	0x00, "Very difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* Trackball 1P */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER1, 100, 63, 0x7f, 0, 0)
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1, 100, 63, 0x7f, 0, 0)

	/* Trackball 2P */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_REVERSE | IPF_PLAYER2, 100, 63, 0x7f, 0, 0)
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2, 100, 63, 0x7f, 0, 0)
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,			/* 8 x 8 characters */
	0x40000/32,		/* 8192 characters */
	4,				/* 4bpp */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8			/* every character takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,8,			/* 8*8 sprites */
	0x40000/32,		/* 8192 sprites */
	4,				/* 4 bpp */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8			/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x000000, &charlayout,     0,	1 },	/* colors 00..31 */
	{ 1, 0x040000, &spritelayout,2*16,	16 },	/* colors 32..47 but using lookup table */
	{ -1 } /* end of array */
};

/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,						/* 1 chip */
	3579545,				/* 3.579545 MHz? */
	{ YM2203_VOL(45,45) },
	AY8910_DEFAULT_GAIN,
	{ 0 },
	{ 0 },
	{ bladestl_port_A_w },	/* uPD7759 (chip 0) */
	{ bladestl_port_B_w }	/* uPD7759 (chip 1)??? */
};

static struct UPD7759_interface upd7759_interface =
{
	2,							/* number of chips */
	UPD7759_STANDARD_CLOCK,
	{ 60, 50 },					/* volume */
	{ 3, 4 },					/* memory regions */
	UPD7759_STANDALONE_MODE,
	{ 0 }
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_HD6309,
			3000000,		/* 24MHz/8 (?) */
			REGION_CPU1,
			bladestl_readmem,bladestl_writemem,0,0,
            bladestl_interrupt,1
        },
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			2000000,		/* ? */
			REGION_CPU2,
			bladestl_readmem_sound, bladestl_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	48, 48 + 16*16,
	bladestl_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	bladestl_vh_start,
	bladestl_vh_stop,
	bladestl_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_UPD7759,
			&upd7759_interface
		}
	}
};

/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( bladestl )
	ROM_REGIONX( 0x18000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "797t01.bin", 0x10000, 0x08000, 0x89d7185d )	/* fixed ROM */
	ROM_CONTINUE(			0x08000, 0x08000 )				/* banked ROM */

	ROM_REGIONX( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "797a05",	0x000000, 0x40000, 0x5491ba28 )	/* tiles */
	ROM_LOAD( "797a06",	0x040000, 0x40000, 0xd055f5cc )	/* sprites */

	ROM_REGIONX( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "797c02", 0x08000, 0x08000, 0x65a331ea )

	ROM_REGION( 0x80000 ) /* uPD7759 data (chip 1) */
	ROM_LOAD( "797a03", 0x00000, 0x80000, 0x9ee1a542 )

	ROM_REGION( 0x40000 ) /* uPD7759 data (chip 2) */
	ROM_LOAD( "797a04",	0x000000, 0x40000, 0x9ac8ea4e )

	ROM_REGIONX( 0x0100, REGION_PROMS )
	ROM_LOAD( "797a07", 0x0000, 0x0100, 0x7aecad4e ) /* sprites lookup table */
ROM_END

ROM_START( bladstle )
	ROM_REGIONX( 0x18000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "797e01", 0x10000, 0x08000, 0xf8472e95 )	/* fixed ROM */
	ROM_CONTINUE(		0x08000, 0x08000 )				/* banked ROM */

	ROM_REGIONX( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "797a05",	0x000000, 0x40000, 0x5491ba28 )	/* tiles */
	ROM_LOAD( "797a06",	0x040000, 0x40000, 0xd055f5cc )	/* sprites */

	ROM_REGIONX( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "797c02", 0x08000, 0x08000, 0x65a331ea )

	ROM_REGION( 0x80000 ) /* uPD7759 data (chip 1) */
	ROM_LOAD( "797a03", 0x00000, 0x80000, 0x9ee1a542 )

	ROM_REGION( 0x40000 ) /* uPD7759 data (chip 2) */
	ROM_LOAD( "797a04",	0x000000, 0x40000, 0x9ac8ea4e )

	ROM_REGIONX( 0x0100, REGION_PROMS )
	ROM_LOAD( "797a07", 0x0000, 0x0100, 0x7aecad4e )	/* lookup table */
ROM_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

struct GameDriver driver_bladestl =
{
	__FILE__,
	0,
	"bladestl",
	"Blades of Steel (version T)",
	"1987",
	"Konami",
	"Manuel Abadia",
	0,
	&machine_driver,
	0,

	rom_bladestl,
	0, 0,
	0,
	0,

	input_ports_bladestl,

	0, 0, 0,
    ORIENTATION_ROTATE_90 | GAME_NOT_WORKING,
	0, 0
};

struct GameDriver driver_bladstle =
{
	__FILE__,
	&driver_bladestl,
	"bladstle",
	"Blades of Steel (version E)",
	"1987",
	"Konami",
	"Manuel Abadia",
	0,
	&machine_driver,
	0,

	rom_bladstle,
	0, 0,
	0,
	0,

	input_ports_bladstle,

	0, 0, 0,
    ORIENTATION_ROTATE_90 | GAME_NOT_WORKING,
	0, 0
};

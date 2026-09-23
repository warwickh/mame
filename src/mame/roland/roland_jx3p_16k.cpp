// license:BSD-3-Clause
// copyright-holders:AJR
/****************************************************************************

    Skeleton driver for Roland JX-3P synthesizer and similar modules.

****************************************************************************/

#include "emu.h"
#include "bus/generic/carts.h"
#include "bus/generic/slot.h"
#include "bus/midi/midi.h"
#include "cpu/mcs51/i8051.h"
#include "machine/adc0804.h"
#include "machine/nvram.h"
#include "pg200.h"
#include "machine/pit8253.h"
#include "machine/rescap.h"
#include "emuopts.h"
#include "output.h"

namespace {

class roland_jx3p_state : public driver_device
{
public:
roland_jx3p_state(const machine_config &mconfig, device_type type, const char *tag)
	: driver_device(mconfig, type, tag)
	, m_maincpu(*this, "maincpu")
	, m_ifcpu(*this, "ifcpu")
	, m_cartslot(*this, "cartslot")
	, m_ifpit(*this, "ifpit%u", 0U)
	, m_ledline(*this, "ledline_%u", 0U)
	, m_ledscline(*this, "ledscline_%u", 0U)
	, m_led(*this, "led_%u", 1U)
	, m_dac(*this, "dac_%u", 0U)
	, m_vco_cv(*this, "vco_cv")
	, m_dacbit(*this, "dacbit_%u", 0U)
	, m_vco_bar(*this, "vco_bar_%u", 0U)
{
}

	void jx3p_16k(machine_config &config);

private:
	u8 m_midi_rxd = 1;
    u8 m_edit_slider = 127;

	void midi_rx_w(int state);

	void prescale_w(u8 data);
	void dac_w(offs_t offset, u8 data);
	void sw_interface_w(u8 data);
	void led_display_w(offs_t offset, u8 data);
	void analog_select_w(u8 data);
	void if_anlg_mux_w(u8 data);

	void prog_map(address_map &map) ATTR_COLD;
	void common_ext_map(address_map &map) ATTR_COLD;
	void jx3p_ext_map(address_map &map) ATTR_COLD;
	void if_prog_map(address_map &map) ATTR_COLD;
	void if_ext_map(address_map &map) ATTR_COLD;
    void update_edit_slider();
	void check_group_b();
	u8 port3_r();
	void port2_w(u8 data);
	void port3_w(u8 data);
    u8 m_mux_channel = 0;
	u8 m_port3_in = 0xff;
    u8 m_last_mux = 0;
	u8 m_current_dac_value = 0;

	required_device<mcs51_cpu_device> m_maincpu;
	optional_device<mcs51_cpu_device> m_ifcpu;
	optional_device<generic_cartslot_device> m_cartslot;
	optional_device_array<pit8253_device, 4> m_ifpit;

	std::array<u8, 4> m_led_matrix{};
	
	output_finder<8> m_ledline;
	output_finder<4> m_ledscline;
	output_finder<32> m_led;
    output_finder<16> m_dac;
    output_finder<> m_vco_cv;
	output_finder<128> m_dacbit;
	output_finder<16> m_vco_bar;

	protected:
	virtual void machine_start() override;

};

void roland_jx3p_state::midi_rx_w(int state)
{
	m_midi_rxd = state ? 1 : 0;

	// Temporary diagnostic logging:
	logerror("MIDI RX=%u PC=%04X\n",m_midi_rxd,	unsigned(m_maincpu->pc()));
}

void roland_jx3p_state::prescale_w(u8 data)
{
}

void roland_jx3p_state::dac_w(offs_t offset, u8 data) 
{ 
	unsigned const channel = (offset >> 4) & 0x0f; 
	unsigned const base = channel * 8; 
	for (unsigned bit = 0; bit < 8; bit++) 
	{
		m_dacbit[base + bit] = BIT(data, bit);
	}
	m_dac[channel] = data; 
	if (channel == 2) 
	{
		m_current_dac_value = data; 
		if (m_last_mux == 0x61) 
		{
			//machine().debug_break();
			m_vco_cv = m_current_dac_value;
			for (int i = 0; i < 16; i++)
			{
				m_vco_bar[i] =(m_current_dac_value >= ((i + 1) * 16));
			}
			logerror("SENS slider=%02X dac=%02X comparator=%u\n", m_edit_slider,m_current_dac_value,m_edit_slider >= m_current_dac_value);
		}
	}
}


void roland_jx3p_state::sw_interface_w(u8 data)
{
    logerror("SW %02X\n", data);
}

[[maybe_unused]] void roland_jx3p_state::if_anlg_mux_w(u8 data)
{
}

u8 roland_jx3p_state::port3_r() 
{ 
	u8 value = 0xff; 
	if (!m_midi_rxd) value &= ~0x01; 
	if (m_last_mux == 0x61) 
	{ 
		bool const comparator = m_edit_slider >= m_current_dac_value; 
		if (!comparator) value &= ~0x10; 
	} 
	return value; 
}

void roland_jx3p_state::analog_select_w(u8 data)
{
	update_edit_slider();
    check_group_b();
	m_last_mux = data & 0x7f;
	m_mux_channel =
		(BIT(data, 2) << 2) |
		(BIT(data, 1) << 1) |
		 BIT(data, 0);

	logerror("MUX P6..P0=%u%u%u%u%u%u%u slider=%u\n",
    BIT(data, 6),
    BIT(data, 5),
    BIT(data, 4),
    BIT(data, 3),
    BIT(data, 2),
    BIT(data, 1),
    BIT(data, 0),
    m_edit_slider);
}


void roland_jx3p_state::port2_w(u8 data)
{
	logerror("P2=%02X\n", data);
}

void roland_jx3p_state::port3_w(u8 data)
{
	logerror(
		"P3=%02X  RXD=%u TXD=%u INT0=%u INT1=%u T0=%u T1=%u WR=%u RD=%u\n",
		data,
		BIT(data,0),
		BIT(data,1),
		BIT(data,2),
		BIT(data,3),
		BIT(data,4),
		BIT(data,5),
		BIT(data,6),
		BIT(data,7));
}

void roland_jx3p_state::prog_map(address_map &map)
{
	map.global_mask(0x3fff);
	map(0x0000, 0x3fff).rom().region("program", 0);
}

void roland_jx3p_state::common_ext_map(address_map &map)
{
	map(0x0000, 0x0000).mirror(0x1ff).w(FUNC(roland_jx3p_state::prescale_w));
	map(0x0200, 0x0203).mirror(0x1fc).w("counter1", FUNC(pit8253_device::write));
	map(0x0400, 0x0403).mirror(0x1fc).w("counter2", FUNC(pit8253_device::write));
	map(0x0600, 0x0603).mirror(0x1fc).w("counter3", FUNC(pit8253_device::write));
	map(0x0800, 0x0803).mirror(0x1fc).w("counter4", FUNC(pit8253_device::write));
	map(0x0a00, 0x0a00).mirror(0x10f).select(0xf0).w(FUNC(roland_jx3p_state::dac_w));
	map(0x0c08, 0x0c08).mirror(0x1f0).portr("SWSCN0");
	map(0x0c09, 0x0c09).mirror(0x1f0).portr("SWSCN1");
	map(0x0c0a, 0x0c0a).mirror(0x1f0).portr("SWSCN2");
	map(0x0c0b, 0x0c0b).mirror(0x1f0).portr("SWSCN3");
	map(0x0e00, 0x0e00).mirror(0x10f).select(0xf0).w(FUNC(roland_jx3p_state::led_display_w));
}

void roland_jx3p_state::jx3p_ext_map(address_map &map)
{
	map.global_mask(0x1fff);
	common_ext_map(map);
	map(0x0c00, 0x0c00).mirror(0x1f0).portr("KYSCN0");
	map(0x0c01, 0x0c01).mirror(0x1f0).portr("KYSCN1");
	map(0x0c02, 0x0c02).mirror(0x1f0).portr("KYSCN2");
	map(0x0c03, 0x0c03).mirror(0x1f0).portr("KYSCN3");
	map(0x0c04, 0x0c04).mirror(0x1f0).portr("KYSCN4");
	map(0x0c05, 0x0c05).mirror(0x1f0).portr("KYSCN5");
	map(0x0c06, 0x0c06).mirror(0x1f0).portr("KYSCN6");
	map(0x0c07, 0x0c07).mirror(0x1f0).portr("KYSCN7");
	map(0x0c0c, 0x0c0c).mirror(0x1f0).portr("SWSCN4");
	map(0x1000, 0x1000).mirror(0x1ff).w(FUNC(roland_jx3p_state::sw_interface_w));
	map(0x1800, 0x1fff).ram().share("nvram");
}

[[maybe_unused]] void roland_jx3p_state::if_prog_map(address_map &map)
{
	map.global_mask(0x1fff);
	map(0x0000, 0x1fff).rom().region("interface", 0);
}

[[maybe_unused]] void roland_jx3p_state::if_ext_map(address_map &map)
{
	map.global_mask(0xfff);
	map(0x000, 0x7ff).ram();
	map(0x800, 0x803).mirror(0x788).rw(m_ifpit[0], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x810, 0x813).mirror(0x788).rw(m_ifpit[1], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x820, 0x823).mirror(0x788).rw(m_ifpit[2], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x830, 0x833).mirror(0x788).rw(m_ifpit[3], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x840, 0x840).mirror(0x78f).rw("adc", FUNC(adc0803_device::read), FUNC(adc0803_device::write));
}


static INPUT_PORTS_START(jx3p_16k)

    PORT_START("SWSCN0")
    PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 1")
    PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 2")
    PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 3")
    PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 4")
    PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 5")
    PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 6")
    PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 7")
    PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 8")

    PORT_START("SWSCN1")
    PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 9")
    PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 10")
    PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 11")
    PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 12")
    PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 13")
    PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 14")
    PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 15")
    PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Step 16")

    PORT_START("SWSCN2")
    PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Chorus")
    PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Mute")
    PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Hold")
    PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Transpose")
    PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Bank A")
    PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Bank B")
    PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Bank C")
    PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Bank D")

    PORT_START("SWSCN3")
    PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Group A")
    PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Group B")
    PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Edit Write")
    PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Sequencer Write")
    PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Note Tie")
    PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Note Rest")
    PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Start/Stop")
    PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Tape Memory")

    PORT_START("SWSCN4")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNUSED)

    PORT_START("KYSCN0")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN1")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN2")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN3")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN4")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN5")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN6")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

    PORT_START("KYSCN7")
    PORT_BIT(0xff, IP_ACTIVE_LOW, IPT_UNKNOWN)

	PORT_START("EDITSLIDER")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_OTHER)
		PORT_NAME("Slider Min")

	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_OTHER)
		PORT_NAME("Slider Mid")

	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_OTHER)
		PORT_NAME("Slider Max")

INPUT_PORTS_END

void roland_jx3p_state::check_group_b()
{
    static bool last = false;

    bool current =
        !(ioport("SWSCN3")->read() & 0x02);

    if (current && !last)
        logerror("GROUP B PRESSED\n");

    last = current;
}

void roland_jx3p_state::update_edit_slider()
{
	static u8 last = 0;

	u8 const v = ioport("EDITSLIDER")->read();
	u8 const changed = v & ~last;

	if (changed & 0x01)
	{
		m_edit_slider = 0;
		logerror("EDIT SLIDER LOW\n");
	}

	if (changed & 0x02)
	{
		m_edit_slider = 127;
		logerror("EDIT SLIDER MID\n");
	}

	if (changed & 0x04)
	{
		m_edit_slider = 254;
		logerror("EDIT SLIDER HIGH\n");
	}

	last = v;
}

void roland_jx3p_state::led_display_w(offs_t offset, u8 data)
{
	unsigned row;
	/*
	    Address lines A4-A7 select one active-low LED scan line.
	    A7-A4 = 1110 -> LEDSC0
	    A7-A4 = 1101 -> LEDSC1
	    A7-A4 = 1011 -> LEDSC2
	    A7-A4 = 0111 -> LEDSC3
	    D0-D7 are latched as LED0-LED7.
	*/
	switch (offset & 0xf0)
	{
	case 0xe0:
		row = 0; // LEDSC0
		break;
	case 0xd0:
		row = 1; // LEDSC1
		break;
	case 0xb0:
		row = 2; // LEDSC2
		break;
	case 0x70:
		row = 3; // LEDSC3
		break;
	default:
		logerror("Unknown LED address A7-A4=%X offset=%02X data=%02X\n",unsigned((offset >> 4) & 0x0f),unsigned(offset),data);
		return;
	}
	/* Store the D0-D7 value latched for this LEDSC line. */
	m_led_matrix[row] = data;
	/* Diagnostic outputs showing which decoded LEDSC line was
	    selected by A4-A7 for the most recent write. */
	for (unsigned scan = 0; scan < 4; scan++)
		m_ledscline[scan] = (scan == row);
	/* Diagnostic outputs showing the current D0-D7 data bus value.
	    These correspond directly to LED0-LED7. */
	for (unsigned column = 0; column < 8; column++)
		m_ledline[column] = BIT(data, column);

	/* Update the stable 4 x 8 matrix display from the four
	    latched row bytes.
	         LED0 LED1 LED2 LED3 LED4 LED5 LED6 LED7
	    SC0   LED1  LED2  LED3  LED4  LED5  LED6  LED7  LED8
	    SC1   LED9  LED10 LED11 LED12 LED13 LED14 LED15 LED16
	    SC2   LED17 LED18 LED19 LED20 LED21 LED22 LED23 LED24
	    SC3   LED25 LED26 LED27 LED28 LED29 LED30 LED31 LED32 */

	for (unsigned scan = 0; scan < 4; scan++)
	{
		unsigned const base = scan * 8;

		for (unsigned column = 0; column < 8; column++)
			m_led[base + column] =
					BIT(m_led_matrix[scan], column);
	}
	logerror("LED LEDSC%u A7-A4=%X LED7-0=%02X\n",row,unsigned((offset >> 4) & 0x0f),data);
}

void roland_jx3p_state::machine_start()
{
	m_led_matrix.fill(0x00); 
	m_midi_rxd = 1; 
	m_edit_slider = 127; 
	m_current_dac_value = 0; 
	m_last_mux = 0; 
	save_item(NAME(m_led_matrix)); 
	save_item(NAME(m_midi_rxd)); 
	save_item(NAME(m_edit_slider)); 
	save_item(NAME(m_current_dac_value)); 
	save_item(NAME(m_last_mux));
	for (unsigned column = 0; column < 8; column++)
		m_ledline[column] = 0;

	for (unsigned scan = 0; scan < 4; scan++)
		m_ledscline[scan] = 0;

	for (unsigned led = 0; led < 32; led++)
		m_led[led] = 0;
}

void roland_jx3p_state::jx3p_16k(machine_config &config)
{
	I8031(config, m_maincpu, 12_MHz_XTAL);

	m_maincpu->set_addrmap(AS_PROGRAM, &roland_jx3p_state::prog_map);
	m_maincpu->set_addrmap(AS_DATA, &roland_jx3p_state::jx3p_ext_map);
	m_maincpu->port_out_cb<1>().set(FUNC(roland_jx3p_state::analog_select_w));
    //m_maincpu->port_in_cb<3>().set([]() { return 0xff; });
	m_maincpu->port_in_cb<3>().set(FUNC(roland_jx3p_state::port3_r));
    m_maincpu->port_out_cb<2>().set(FUNC(roland_jx3p_state::port2_w));
    m_maincpu->port_out_cb<3>().set(FUNC(roland_jx3p_state::port3_w));
    
	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0); // TC5517APL + battery

	PIT8253(config, "counter1");
	PIT8253(config, "counter2");
	PIT8253(config, "counter3");
	PIT8253(config, "counter4");

	PG200(config, "programmer");
    midi_port_device &mdin(MIDI_PORT(config, "mdin", midiin_slot, "midiin"));
    mdin.rxd_handler().set(FUNC(roland_jx3p_state::midi_rx_w));
    //config.set_default_layout(layout_jx3p_16k);
}


ROM_START(jx3p_16k)
	ROM_REGION(0x4000, "program", 0)
	ROM_LOAD("jx3p_vel.bin", 0x0000, 0x4000, CRC(6913cab3) SHA1(dced1805ab7ba87f588e8e4a087680c038dd13f2))
ROM_END

} // anonymous namespace


SYST(1983, jx3p_16k,  0, 0, jx3p_16k,  jx3p_16k,  roland_jx3p_state, empty_init, "Roland", "JX-3P Programmable Preset Polyphonic Synthesizer - Velocity ROM", MACHINE_NO_SOUND | MACHINE_NOT_WORKING)

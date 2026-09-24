// license:BSD-3-Clause
// copyright-holders:AJR
/****************************************************************************

    Skeleton device for Roland PG-200 programmer.

****************************************************************************/

// license:BSD-3-Clause
// copyright-holders:AJR

#include "emu.h"
#include "pg200.h"

DEFINE_DEVICE_TYPE(PG200, pg200_device, "pg200", "Roland PG-200 Programmer")

pg200_device::pg200_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, PG200, tag, owner, clock)
	, m_pgcpu(*this, "pgcpu")
	, m_tx_cb(*this)
{
}

void pg200_device::p2_w(u8 data)
{
    int const new_tx_state = BIT(data, 7);

    if (new_tx_state != m_tx_state)
    {
        attotime const now = machine().time();
        attotime const elapsed = now - m_last_tx_change;

        logerror(
            "PG200 TX %d -> %d  time=%s  elapsed=%s  P2=%02X\n",
            m_tx_state,
            new_tx_state,
            now.to_string(),
            elapsed.to_string(),
            data);

        m_tx_state = new_tx_state;
        m_last_tx_change = now;
    }

    m_p2 = data;
}

void pg200_device::device_start()
{
    m_p2 = 0xff;
    m_tx_state = 1;
    m_last_tx_change = machine().time();

    save_item(NAME(m_p2));
    save_item(NAME(m_tx_state));
    save_item(NAME(m_last_tx_change));
}

void pg200_device::device_add_mconfig(machine_config &config)
{
	I8048(config, m_pgcpu, 6_MHz_XTAL);
	m_pgcpu->p2_out_cb().set(FUNC(pg200_device::p2_w));
	//m_pgcpu->p2_out_cb().set(FUNC(pg200_device::p2_w));
}

ROM_START(pg200)
	ROM_REGION(0x400, "pgcpu", 0)
	ROM_LOAD("m5l8048-067p_b4d4.ic1", 0x000, 0x400, CRC(4306aad7) SHA1(145e12e5cf22b6db4958651a04d892f4a4215bb1))
ROM_END

const tiny_rom_entry *pg200_device::device_rom_region() const
{
	return ROM_NAME(pg200);
}

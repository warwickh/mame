// license:BSD-3-Clause
// copyright-holders:AJR

#ifndef MAME_ROLAND_PG200_H
#define MAME_ROLAND_PG200_H

#include "cpu/mcs48/mcs48.h"


class pg200_device : public device_t
{
public:
	pg200_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);
	auto tx_callback() { return m_tx_cb.bind(); }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;

private:
	required_device<mcs48_cpu_device> m_pgcpu;
	void p2_w(u8 data);
    devcb_write_line m_tx_cb;
	u8 m_p2 = 0xff;
    int m_tx_state = 1;
    attotime m_last_tx_change;
};

DECLARE_DEVICE_TYPE(PG200, pg200_device)

#endif // MAME_ROLAND_PG200_H

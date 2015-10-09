/* Digital Audio Interface clocking API.*/
int dai_set_sysclk(int dai, int clk_id, unsigned int freq, int dir);

int dai_set_clkdiv(int dai, int div_id, int div);

int dai_set_pll(int dai, int pll_id, int source, unsigned int freq_in,
	unsigned int freq_out);

int dai_set_bclk_ratio(int dai, unsigned int ratio);

/* Digital Audio interface formatting */
int dai_set_fmt(int dai, unsigned int fmt);

int dai_set_tdm_slot(int dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width);

int dai_set_channel_map(int dai,
	unsigned int tx_num, unsigned int *tx_slot,
	unsigned int rx_num, unsigned int *rx_slot);

int dai_set_tristate(int dai, int tristate);

/* Digital Audio Interface mute */
int dai_digital_mute(int dai, int mute, int direction)
{
	return dai_ops[dai].mute(dai, mute, direction);
}

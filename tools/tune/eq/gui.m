clear;
global EQ = [];
global main_panes;
global fn = 1;
% TODO move to external GUI control?
global Fs = 48000;
global nyquist_f = Fs/2;

function peq = build_peq(channel)
	global EQ;
	peq = [];
	for i = 1:8
		biquad = EQ(i, channel);
		if get(biquad.enable_switch, "value") == 1
			type = get(biquad.type.dropdown, "value");
			freq = str2num(get(biquad.freq.edit, "string"));
			Q = str2double(get(biquad.Q.edit, "string"));
			gain = str2double(get(biquad.gain.edit, "string"));
			% dropdown maps straight to filter index
			% filter only uses needed arguments, all others will be ignored
			peq = [peq; type, freq, gain, Q];
		endif
	endfor
end

function update_plot(channel)
	global main_panes;
	global nyquist_f
	global Fs
	pl = main_panes(channel).plot;
	spectrum_size = 1000;
	n = logspace(log10(1), log10(nyquist_f), spectrum_size);
	peq = build_peq(channel);
	[z p k] = eq_define_parametric_eq(peq, Fs);
	% octave gets angry when you pile up poles and zeros
	[num den] = zp2tf(z, p, k);
	[h, w] = freqz(num,den, n, Fs);
	h = 20*log10(h);
	set(pl, "YData", h, "XData", log2(w));
end

function set_section_visibility(section, visible)
	for [control, control_name] = section
		set(control, "visible", visible);
	endfor
end

function dropdown_callback(h, event)
	global EQ;
	udata = get(h, "userdata");
	biquad  = EQ(udata.index, udata.channel);
	eq = eq_defaults();
	switch (get(h, "value"))
		case eq.PEQ_HP1
			% highpass 1st order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_HP2
			% highpass 2nd order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_LP1
			% lowpass 1st order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_LP2
			% lowpass 2nd order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_LS1
			% lowshelf 1st order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_LS2
			% lowhelf 2nd order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_HS1
			% highshelf 1st order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_HS2
			% highshelf 2nd order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_PN2
			% peaking
			set_section_visibility(biquad.Q, "on");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_LP4
			% lowpass 4th order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_HP4
			% highpass 4th order
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_LP2G
			% lowpass 2nd order with resonnance
			set_section_visibility(biquad.Q, "on");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_HP2G
			% highpass 2nd order with resonnance
			set_section_visibility(biquad.Q, "on");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_BP2
			% bandpass
			set_section_visibility(biquad.Q, "on");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_NC2
			% notch
			set_section_visibility(biquad.Q, "on");
			set_section_visibility(biquad.gain, "off");
		case eq.PEQ_LS2G
			% lowshelf, CRAS implementation
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
		case eq.PEQ_HS2G
			% highshelf, CRAS implementation
			set_section_visibility(biquad.Q, "off");
			set_section_visibility(biquad.gain, "on");
	endswitch
	update_plot(udata.channel);
end

function enable_callback(h, event)
	global EQ;
	off = get(h, "value") == 0;
	if off
		visible = "off";
	else
		visible = "on";
	endif
	udata = get(h, "userdata");
	biquad  = EQ(udata.index, udata.channel);
	for [section, section_name] = biquad
		if strcmp(section_name, "enable_switch") || strcmp(section_name, "pane")
			continue
		endif
		set_section_visibility(section, visible);
	endfor
	if !off
		dropdown_callback(biquad.type.dropdown,0);
	endif
	update_plot(udata.channel);
end

function slider_callback(h, event)
	global EQ;
	udata = get(h, "userdata");
	editbox  = getfield(EQ(udata.index, udata.channel), udata.prefix).edit;
	val = get(h, "value");
	if udata.is_log
		val = 0.1*2^val;
	endif
	set(editbox, "string", num2str(val));
	update_plot(udata.channel);
end

function edit_callback(h, event)
	global EQ;
	udata = get(h, "userdata");
	slider  = getfield(EQ(udata.index, udata.channel), udata.prefix).slider;
	val = str2num(get(h, "string"));
	if udata.is_log && val != 0
		val = log2(val/0.1);
	end
	set(slider, "value", val);
	update_plot(udata.channel);
end

function initialize_biquad(index, channel, position, main_panes)
	global EQ;
	global nyquist_f
	% this string matches the order defined in eq.PEQ_*, do not change the order
	eq_strings = {"highpass 1st order", "highpass 2nd order", ...
		"lowpass 1st order", "lowpass 2nd order", "lowshelf 1st order", ...
		"lowshelf 2nd order", "highshelf 1st order", "highshelf 2nd order", ...
		"peaking 2nd order", "lowpass 4th order", "highpass 4th order", ...
		"lowpass 2nd order (Google)", "highpass 2nd order (Google)", ...
		"bandpass 2nd order", "notch 2nd order", "lowshelf 2nd order (Google)", ...
		"highshelf 2nd order (Google)"};
	eq = eq_defaults();
	% UI panels, we have to use absolute positioning
	%
	% +-------+-----------+-------+--+--+--+--+--+----------+
	% | Label | Dropdown  |       |  |  |  |  |  | checkbox |
	% +-------+-----------+-------+--+--+--+--+--+----------+
	% | Label | input box | label | slider                  |
	% +-------+-----------+-------+-------------------------+
	% | Label | input box | label | slider                  |
	% +-------+-----------+-------+-------------------------+
	% | Label | input box | label | slider                  |
	% +-------+-----------+-------+-------------------------+

	EQ(index, channel).pane = uipanel ("parent", main_panes(channel).pane, "position", position);
	p = EQ(index, channel).pane;
	% octave is silly and doesn't let one position controls relatively inside a uipanel
	EQ(index, channel).gain.label = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"position", [0 10 40 30], ...
		"visible", "off", ...
		"string", "Gain");
	EQ(index, channel).gain.edit = uicontrol( ...
		"style", "edit", ...
		"parent", p, ...
		"position", [40 10 90 30], ...
		"string", "0", ...
		"userdata", struct("index", index, "channel", channel, "prefix", "gain", "is_log", false), ...
		"visible", "off", ...
		"callback", @edit_callback);
	EQ(index, channel).gain.units = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"string", "dB", ...
		"visible", "off", ...
		"position", [140 10 17 30]);
	EQ(index, channel).gain.slider = uicontrol( ...
		"style", "slider", ...
		"parent", p, ...
		"position", [160 10 200 30], ...
		"min", -40, ...
		"max", 40, ...
		"userdata", struct("index", index, "channel", channel, "prefix", "gain", "is_log", false), ...
		"visible", "off", ...
		"callback", @slider_callback);

	EQ(index, channel).Q.label = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"position", [0 50 50 30], ...
		"visible", "off", ...
		"string", "Q");
	EQ(index, channel).Q.edit = uicontrol( ...
		"style", "edit", ...
		"parent", p, ...
		"position", [40 50 90 30], ...
		"string", "1", ...
		"userdata", struct("index", index, "channel", channel, "prefix", "Q", "is_log", true), ...
		"visible", "off", ...
		"callback", @edit_callback);
	EQ(index, channel).Q.slider = uicontrol( ...
		"style", "slider",                 ...
		"parent", p,                       ...
		"position", [160 50 200 30],       ...
		"min", 0,                          ...
		"max", log2(1000/0.1),             ...
		"value", log2(1/0.1),                        ...
		"userdata", struct("index", index, "channel", channel, "prefix", "Q", "is_log", true), ...
		"visible", "off", ...
		"callback", @slider_callback);

	EQ(index, channel).freq.label = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"position", [0 90 40 30], ...
		"visible", "off", ...
		"string", "Freq");
	EQ(index, channel).freq.edit = uicontrol( ...
		"style", "edit", ...
		"parent", p, ...
		"position", [40 90 90 30], ...
		"string", "350", ...
		"userdata", struct("index", index, "channel", channel, "prefix", "freq", "is_log", true), ...
		"visible", "off", ...
		"callback", @edit_callback);
	EQ(index, channel).freq.units = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"position", [140 90 17 30], ...
		"visible", "off", ...
		"string", "Hz");
	EQ(index, channel).freq.slider   = uicontrol( ...
		"style", "slider", ...
		"parent", p, ...
		"position", [160 90 200 30], ...
		"min", 1, ...
		"max", log2(nyquist_f/0.1), ...
		"value", log2(350/0.1), ...
		"userdata", struct("index", index, "channel", channel, "prefix", "freq", "is_log", true), ...
		"visible", "off", ...
		"callback", @slider_callback);

	EQ(index, channel).type.label = uicontrol( ...
		"style", "text", ...
		"parent", p, ...
		"position", [0 130 40 30], ...
		"visible", "off", ...
		"string", "Type");
	EQ(index, channel).type.dropdown = uicontrol( ...
		"style", "popupmenu", ...
		"parent", p, ...
		"position", [40 130 240 30], ...
		"string", eq_strings, ...
		"value", eq.PEQ_PN2, ...
		"userdata", struct("index", index, "channel", channel), ...
		"visible", "off", ...
		"callback", @dropdown_callback);
	EQ(index, channel).enable_switch = uicontrol( ...
		"style", "checkbox", ...
		"parent", p, ...
		"position", [350 130 30 30], ...
		"value", 0, ...
		"userdata", struct("index", index, "channel", channel), ...
		"callback", @enable_callback);
end

pkg load signal;

% create figure and panel on it
fn = figure(fn, "toolbar", "none", "menubar", "none", 'Position', [200 200 1500 700]); fn = fn + 1;
EQ_right = uipanel ("title", "EQ Right", "position", [0, 0, 1, 0.5]);
EQ_left = uipanel ("title", "EQ Left", "position", [0, 0.5, 1, 0.5]);
main_panes = [struct("pane", EQ_left); struct("pane", EQ_right)];

% UI panel relative position table
positions = [
	[0    0   0.25 0.5 ]
	[0.25 0   0.5  0.5 ]
	[0.5  0   0.75 0.5 ]
	[0.75 0   1    0.5 ]
	[0    0.5 0.25 1   ]
	[0.25 0.5 0.5  1   ]
	[0.5  0.5 0.75 1   ]
	[0.75 0.5 1    1   ]
];

for c = 1:2
	for i = 1:length(positions)
		initialize_biquad(i, c, positions(i, 1:end), main_panes);
	endfor
endfor

fn = figure(fn); fn = fn + 1;
h = [0 0];
w = [1 nyquist_f];
n_octaves = 10;
subplot(2, 1, 1);
main_panes(1).plot = plot(log2(w), h, "linewidth", 4);
% Range set to match http://audio-tuning.appspot.com/
axis([log2(nyquist_f / (2 ^ n_octaves)), log2(nyquist_f), -24, 18]);
grid on;
subplot(2, 1, 2);
main_panes(2).plot = plot(log2(w), h, "linewidth", 4);
% Range set to match http://audio-tuning.appspot.com/
axis([log2(nyquist_f / (2 ^ n_octaves)), log2(nyquist_f), -24, 18]);
grid on;
% TODO add ability to switch between octaves and log(Hz)
fn = figure(fn, "toolbar", "none", "menubar", "none", 'Position', [200 200 260 130]); fn = fn + 1;
menu_window = struct();
menu_window.ip_box = uicontrol( ...
	"style", "edit", ...
	"position", [80 100 180 30]);
menu_window.ip_label = uicontrol( ...
	"style", "text",
	"string", "DUT IP:", ...
	"position", [0 100 80 30]);
menu_window.upload = uicontrol( ...
	"style", "pushbutton", ...
	'string', 'Push config to DUT', ...
	"position", [0 50 260 50]);
menu_window.load_dsp = uicontrol( ...
	"style", "pushbutton", ...
	'string', 'Load dsp.ini', ...
	"callback", @eq_load_dsp_ini, ...
	"position", [0 0 260 50], ...
	"userdata", struct("EQ", EQ));

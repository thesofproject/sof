function eq_load_dsp_ini(h, evnt)

udata = get(h, "userdata");
EQ = udata.EQ;
preamble_offset = 4;
config_size = 4;
n_channels = 2;
eq = eq_defaults();

[name, path, idx] = uigetfile("*.ini");
if name == 0
	% cancelled
	return;
endif

% load file
f = fopen(strcat(path, name));

% locate eq2 section
do
	l = fgets(f);
until strcmp(l, "[eq2]\n") || l == -1

if l == -1
	errordlg("eq2 section not found.\n Please check input file.");
	return;
endif

config = [];

% load config
do
	line = fgets(f);
	if line == -1
		break;
	endif
	[v, count] = sscanf(line, "input_%d = %d");
	if count == 2
		config = [config, [v(1); v(2)]];
	endif
until strcmp("\n", line)

fclose(f);

% push config to GUI

% set all fields to off, we will run the callback at the end to avoid graphics flicker
for channel = 1:2
	for i = 1:8
		enable_switch = EQ(i, channel).enable_switch;
		set(enable_switch, "value", 0);
	endfor
endfor

 % cras type (+1 since its 0 index); sof type
cras_filter2sof_filter = [ ...
	-1, ...  % none
	eq.PEQ_LP2G, ...
	eq.PEQ_HP2G, ...
	eq.PEQ_BP2, ...
	eq.PEQ_LS2G, ...
	eq.PEQ_HS2G, ...
	eq.PEQ_PN2, ...
	eq.PEQ_NC2, ...
	-1, ... % allpass, not implemented in audio-tuning or in SOF
];

config_item2field_name = {
	"type",
	"freq",
	"Q",
	"gain",
};

for r = config
	index = r(1) - preamble_offset;
	config_index =  idivide(index, config_size * n_channels, "floor") + 1;
	config_item = rem(index, config_size) + 1;
	channel = rem(idivide(index, config_size, "floor"), n_channels) + 1;
	field_name = config_item2field_name{config_item};
	filter = EQ(config_index, channel);
	section = getfield(filter, field_name);

	if strcmp(field_name, "type") && cras_filter2sof_filter(r(2) + 1) == -1
		% invalid filter, skip
		continue;
	endif

	if strcmp(field_name, "type")
		% no need to run this callback since it will get called when we run the
		% enable_switch callback at the end
		set(section.dropdown, "value", cras_filter2sof_filter(r(2) + 1));
		set(filter.enable_switch, "value", 1);
	else
		set(section.edit, "string", num2str(r(2)));
		% run callback to update slider value
		func = get(section.edit, "callback");
		func(section.edit, 0);
	endif
endfor

% now that all values are set lets update eveyone's visibility
for channel = 1:2
	for i = 1:8
		enable_switch = EQ(i, channel).enable_switch;
		func = get(enable_switch, "callback");
		func(enable_switch, 0);
	endfor
endfor
end

function [bytes, nbytes] = dcblock_get_abi(setsize)

%% Use sof-ctl to write ABI header into a file
abifn = 'dcblock_get_abi.bin';
cmd = sprintf('sof-ctl -g %d -b -o %s', setsize, abifn);
system(cmd);

%% Read file and delete it
fh = fopen(abifn, 'r');
if fh < 0
	error("Failed to get ABI header. Is sof-ctl installed?");
end
[bytes, nbytes] = fread(fh, inf, 'uint8');
fclose(fh);
delete(abifn);

end

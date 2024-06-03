function status = sof_eq_deploy_to_dut(target, temp_file)
% deploy eq over ssh to a given dut with sof-eqctl
% the end target is expected to be some sort of unix system with sof-eqctl
% already installed. The system should also have ssh keys installed to prevent
% the toolchain from logging in

ip = target.ip;
user = target.user;
dev = target.device;
ctrl = target.control;

if isunix() || ismac()
	transfer_template = "scp %s %s@%s:/tmp/sof-eq-config.txt";
	config_template = "ssh -t %s@%s sof-eqctl -Dhw:%d -n %d -s /tmp/sof-eq-config.txt";
	transfer_command = sprintf(transfer_template, temp_file, user, ip);
	config_command = sprintf(config_template, user, ip, dev, ctrl);
	command = sprintf("%s && %s", transfer_command, config_command);
endif

if ispc()
	% TODO
endif

[status, output] = system(command);
disp(output);
end


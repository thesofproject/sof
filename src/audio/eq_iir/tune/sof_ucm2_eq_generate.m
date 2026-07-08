function sof_ucm2_eq_generate(sys_vendor, product_name, endpoint, meas_file)

% SOF_UCM2_EQ_GENERATE  Fit IIR + FIR endpoint EQ from a measurement.
%
%   sof_ucm2_eq_generate(SYS_VENDOR, PRODUCT_NAME, ENDPOINT, MEAS_FILE)
%
%   SYS_VENDOR   DMI /sys/devices/virtual/dmi/id/sys_vendor value, e.g.
%                'Acme Ltd.'. Kept verbatim (with spaces and case) for the
%                product_configs directory name.
%   PRODUCT_NAME DMI product_name value, e.g. 'Model 100'. Kept
%                verbatim for the .conf file name.
%   ENDPOINT     Endpoint being tuned, e.g. 'speaker' or 'headphone'. Used
%                lower case in blob file names and capitalized in the
%                Define.PostMixer<Endpoint>Playback... UCM keys.
%   MEAS_FILE    Path to a comma separated numbers text file whose first
%                column is the measurement frequency in Hz and whose
%                remaining columns are one or more measured magnitude
%                traces in dB. Multiple traces are averaged.
%                Such a file can be produced with sof_mls_freq_resp.m.
%                Excel workbooks (.xls, .xlsx, .xlsm) and OpenDocument
%                spreadsheets (.ods) are also accepted; the format is
%                detected from the file extension and imported via
%                xlsread from the Octave 'io' package. The workbook
%                is expected to contain the same [freq, resp...] numeric
%                layout with no header row, matching
%                sof_ucm2_eq_example.xlsx.
%
%   The binary IIR and FIR blobs consumed by UCM are written to
%     ./ucm2_blobs_sof/ipc4/eq_iir/<endpoint>_<vendor>_<product>_iir.bin
%     ./ucm2_blobs_sof/ipc4/eq_fir/<endpoint>_<vendor>_<product>_fir.bin
%
%   The matching sof-ctl text dumps for manual testing are written under
%   the SOF source tree (independent of the caller's current directory):
%     <sof>/tools/ctl/ipc4/eq_iir/<endpoint>_<vendor>_<product>_iir.txt
%     <sof>/tools/ctl/ipc4/eq_fir/<endpoint>_<vendor>_<product>_fir.txt
%
%   and a UCM include file that will be picked up automatically by UCM is
%   written to
%     ./ucm2_blobs_sof/product_configs/<SYS_VENDOR>/<PRODUCT_NAME>.conf
%
%   Run this to see a design example and produced configuration for ALSA UCMv2
%     sof_ucm2_eq_generate('example', 'example', 'speaker', 'sof_ucm2_eq_example.txt');
%   or
%     sof_ucm2_eq_generate('example', 'example', 'speaker', 'sof_ucm2_eq_example.xlsx');

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2026, Intel Corporation.

if nargin < 4
	help sof_ucm2_eq_generate
	printf('\n');
	error('Usage: sof_ucm2_eq_generate(sys_vendor, product_name, endpoint, meas_file)');
end

%% Validate DMI and endpoint inputs before they are used to build any
%% path or UCM key. sys_vendor and product_name are kept verbatim as
%% directory / file name components under product_configs/, so they must
%% not contain path separators or traversal segments. endpoint is
%% capitalized and dropped into Define.PostMixer<Endpoint>Playback...
%% keys, so it has to be a plain letter identifier.
validate_dmi_field('sys_vendor', sys_vendor);
validate_dmi_field('product_name', product_name);
validate_endpoint(endpoint);

%% Load the signal package up front so the script fails cleanly on a
%% system missing it, before any output directories are created. The io
%% package is only pulled in when the measurement file is a spreadsheet
%% (see load_measurement below), so text-only users do not need it.
pkg load signal;

%% Derive blob base name and output paths from the DMI info + endpoint.
%% The binary blobs go into a UCM-shaped staging tree (ucm2_blobs_sof/)
%% that mirrors the alsa-ucm-conf layout. The sof-ctl text dumps go into
%% the regular tools/ctl/ipc4/eq_{iir,fir}/ locations so they can be used
%% for manual testing with sof-ctl the same way as the other tune scripts;
%% UCM itself does not consume these .txt files. The tools/ctl path is
%% derived from the location of this script so it works regardless of the
%% caller's current directory.
endpoint_lc = lower(endpoint);
base = sprintf('%s_%s_%s', endpoint_lc, ...
               sanitize_name('sys_vendor', sys_vendor), ...
               sanitize_name('product_name', product_name));
out_root = 'ucm2_blobs_sof';
cpath4 = fullfile(out_root, 'ipc4');
iir_bin = fullfile('eq_iir', [base '_iir.bin']);
fir_bin = fullfile('eq_fir', [base '_fir.bin']);
conf_dir = fullfile(out_root, 'product_configs', sys_vendor);
conf_file = fullfile(conf_dir, [product_name '.conf']);
ensure_dir(fullfile(cpath4, 'eq_iir'));
ensure_dir(fullfile(cpath4, 'eq_fir'));
ensure_dir(conf_dir);

script_dir = fileparts(mfilename('fullpath'));
ctl_root = fullfile(script_dir, '..', '..', '..', '..', 'tools', 'ctl', 'ipc4');
ctl_iir_dir = fullfile(ctl_root, 'eq_iir');
ctl_fir_dir = fullfile(ctl_root, 'eq_fir');
ensure_dir(ctl_iir_dir);
ensure_dir(ctl_fir_dir);
iir_txt = fullfile(ctl_iir_dir, [base '_iir.txt']);
fir_txt = fullfile(ctl_fir_dir, [base '_fir.txt']);

enable_common_paths(script_dir, true);

%% Base equalizer setup
eq = sof_eq_defaults();
eq.fs = 48e3;
eq.logsmooth_plot = 1.0;
eq.logsmooth_eq = 1.0;
eq.enable_iir = 1;
eq.enable_fir = 1;
eq.iir_norm_type = 'loudness';
eq.iir_norm_offs_db = -1;
eq.fir_norm_type = 'loudness';
eq.fir_norm_offs_db = -1;
eq.p_fmin = 20;
eq.p_fmax = 20e3;

%% Parametric target: keep the corrected response band-limited so the fit
%% does not chase noise below 80 Hz (below the speaker's useful range) or
%% brighten the top octave. HP2 80 Hz rolls off sub-bass in the target,
%% LP2 10 kHz tames the highs. Both the IIR stage fit (stage_rms) and the
%% FIR design in sof_eq_compute honor eq.t_db that this generates.
eq.parametric_target_response = [ ...
	eq.PEQ_HP2  80    0  0; ...
	eq.PEQ_LP2  10000   0  0; ...
];

%% Load measurement, fit IIR + FIR, compute the final response and export
eq = load_measurement(eq, meas_file);
eq = design_iir_stages(eq);
eq = configure_fir(eq);
eq = sof_eq_compute(eq);
sof_eq_plot(eq, 1);
export_blobs(eq, cpath4, iir_txt, iir_bin, fir_txt, fir_bin);
write_product_conf(conf_file, sys_vendor, product_name, endpoint, base, eq);

enable_common_paths(script_dir, false);

printf('\n');
printf('To install these blobs into alsa-ucm-conf, copy the contents of\n');
printf('%s/ recursively into alsa-ucm-conf/ucm2/blobs/sof:\n', out_root);
printf('    cp -r %s/* <alsa-ucm-conf>/ucm2/blobs/sof/\n', out_root);
printf('sof-ctl text dumps written under tools/ctl/ipc4/eq_{iir,fir}/.\n');

end

%% -----------------------------------------------------------------------
%% Measurement loading
%% -----------------------------------------------------------------------
function eq = load_measurement(eq, meas_file)
if ~exist(meas_file, 'file')
	error('Measurement file not found: %s', meas_file);
end
[~, ~, ext] = fileparts(meas_file);
switch lower(ext)
case {'.xls', '.xlsx', '.xlsm', '.ods'}
	pkg load io;
	meas = xlsread(meas_file);
otherwise
	%% dlmread with no explicit delimiter auto-detects whitespace or
	%% comma separation, which covers both the sof_mls_freq_resp.m
	%% output and legacy whitespace-delimited measurement files.
	meas = dlmread(meas_file);
end
if size(meas, 2) < 2
	error('%s must have at least a frequency column and one response column', ...
	      meas_file);
end
eq.raw_f = meas(:,1);
if size(meas, 2) == 2
	eq.raw_m_db = meas(:,2);
else
	%% Average of the response columns
	eq.raw_m_db = mean(meas(:, 2:end), 2);
	fprintf('Averaged %d response columns from %s\n', size(meas, 2) - 1, meas_file);
end
end

%% -----------------------------------------------------------------------
%% Multi-stage IIR fit
%% -----------------------------------------------------------------------
function eq = design_iir_stages(eq)

%% Cap on the combined response shaping between the mid PN2 (stage 1) and
%  the bass LS2 (stage 2). If stage 1 pulls the mids down by A dB, the
%  LS2 upper bound is reduced so that bass_boost + |mid_atten| does not
%  exceed this limit. This keeps the bass-to-mid tilt from becoming
%  excessive (e.g. 26 dB total is already an aggressive shape).
max_bass_plus_mid_atten_db = 26;

%% Filter budget (4 biquads max):
%   1) HP2 at 80 Hz to protect the speaker
%   2) PN2 mid shaper                    -> fc [2000, 4000], gain [-12, +6],  Q [0.5, 1.0]
%   3) LS2 bass boost                    -> fc [120, 1000],  gain [0,   +12]
%   4) PN2 fine correction (low band)    -> fc [200, 4000],  gain [-6,  +12], Q [0.5, 1.0]
% Fitting order matters: the mid PN2 is fit first so the subsequent low
% shelf can settle on top of an already-flattened midrange instead of
% chasing a mid bump with bass gain. The remaining high-band correction
% is left to the mid-band FIR that runs after the IIR, which keeps the
% IIR order low and avoids the audible ringing seen with a larger IIR
% budget.
hp_fc = 80;

opts = optimset('Display', 'notify', 'MaxIter', 300, 'MaxFunEvals', 2000, ...
                'TolX', 1e-3, 'TolFun', 1e-3);

peq_fixed = [eq.PEQ_HP2, hp_fc, 0, 0];

%% Stage 1: fit a mid PN2 peak/notch first, on top of the HP2. Doing this
%   before the low shelf keeps the shelf from over-compensating for a
%   broad midrange bump. The gain upper bound is kept low (+6 dB) so this
%   stage stays a mid *attenuator* rather than a boost.
%   Params: [fc, gain, Q]. fc [2000, 4000] Hz, gain [-12, +6] dB, Q [0.5, 1.0].
fmin_fit = 400;
fmax_fit = 7000;
p1_0 = [2500, -6, 0.6];
p1_bounds = [2000, 4000; -12, +6;  0.5, 1.0];
p1 = fminsearch(@(p) stage_rms(peq_fixed, pn_row(p, p1_bounds, eq), eq, fmin_fit, fmax_fit), ...
                p1_0, opts);
peq_fixed = [peq_fixed; pn_row(p1, p1_bounds, eq)];
fprintf('Stage 1 (mid PN2): fc=%.1f Hz  g=%.2f dB  Q=%.2f\n', ...
        clamp(p1(1), p1_bounds(1,1), p1_bounds(1,2)), ...
        clamp(p1(2), p1_bounds(2,1), p1_bounds(2,2)), ...
        clamp(p1(3), p1_bounds(3,1), p1_bounds(3,2)));

%% Stage 2: fit the low shelf on top of the flattened midrange.
%   Params: [ls_fc, ls_g]. fc [120, 1000] Hz, gain [0, +12] dB.
%   The upper bass gain bound is shrunk when stage 1 attenuated the mids,
%   so the total bass-to-mid shaping stays within max_bass_plus_mid_atten_db.
fmin_fit = 200;
fmax_fit = 2000;
ls0 = [200, 8];
ls_bounds = [120, 1000;   0, 12];
mid_atten_db = min(0, clamp(p1(2), p1_bounds(2,1), p1_bounds(2,2)));
ls_bounds(2,2) = min(ls_bounds(2,2), max(0, max_bass_plus_mid_atten_db - abs(mid_atten_db)));
if ls_bounds(2,2) < ls0(2)
	ls0(2) = ls_bounds(2,2);
end
fprintf('Stage 2 bass gain upper bound = %.2f dB (mid atten %.2f dB, cap %.1f dB)\n', ...
        ls_bounds(2,2), mid_atten_db, max_bass_plus_mid_atten_db);
ls = fminsearch(@(p) stage_rms(peq_fixed, shelf_row(eq.PEQ_LS2, p, ls_bounds), ...
                                eq, fmin_fit, fmax_fit), ls0, opts);
peq_fixed = [peq_fixed; shelf_row(eq.PEQ_LS2, ls, ls_bounds)];
fprintf('Stage 2 (LS2): fc=%.1f Hz  g=%.2f dB\n', clamp(ls(1), ls_bounds(1,1), ls_bounds(1,2)), ...
        clamp(ls(2), ls_bounds(2,1), ls_bounds(2,2)));

%% Widen the fit band for the fine correction stage: below 400 Hz the LS2
%  already dominates and above the mid we still want to shape the response
%  out to 7 kHz.
fmin_fit = 400;
fmax_fit = 7000;

%% Stage 3: single fine correction across the low/mid band. The higher
%   frequencies are left to the mid-band FIR, so this biquad can focus on
%   whatever residual the shelf + mid PN2 left behind.
%   Params: [fc, gain, Q]. fc [200, 4000] Hz, gain [-6, +12] dB, Q [0.5, 1.0].
p2_0 = [800, -3, 1.0];
p2_bounds = [200, 4000; -6, 12;  0.5, 1.0];
p2 = fminsearch(@(p) stage_rms(peq_fixed, pn_row(p, p2_bounds, eq), eq, fmin_fit, fmax_fit), ...
                p2_0, opts);
peq_fixed = [peq_fixed; pn_row(p2, p2_bounds, eq)];
fprintf('Stage 3 (fine PN2): fc=%.1f Hz  g=%.2f dB  Q=%.2f\n', ...
        clamp(p2(1), p2_bounds(1,1), p2_bounds(1,2)), ...
        clamp(p2(2), p2_bounds(2,1), p2_bounds(2,2)), ...
        clamp(p2(3), p2_bounds(3,1), p2_bounds(3,2)));

eq.peq = peq_fixed;
end

%% -----------------------------------------------------------------------
%% FIR configuration for mid-band residual correction
%% -----------------------------------------------------------------------
function eq = configure_fir(eq)
%% The IIR takes care of the coarse bass shelf plus two low/mid PN2
%  shapers. Whatever residual vs. the target is left after the IIR
%  (fir_compensate_iir = 1 in the defaults) is picked up here by a short
%  minimum-phase FIR limited to [fmin_fir, fmax_fir], so it does not
%  spend taps on the LF/HF regions the IIR already handles or where the
%  measurement is unreliable.
eq.fir_length = 63;
eq.fir_beta = 10;
eq.fir_minph = 1;
eq.fir_autoband = 0;
eq.fmin_fir = 400;
eq.fmax_fir = 7000;
fprintf('FIR: length=%d taps, mid band [%d, %d] Hz\n', ...
        eq.fir_length, eq.fmin_fir, eq.fmax_fir);
end

%% -----------------------------------------------------------------------
%% IIR + FIR blob packing and export
%% -----------------------------------------------------------------------
function export_blobs(eq, cpath, iir_txt, iir_bin, fir_txt, fir_bin)
%% Two-channel blob with a single shared response. Both channels are
%% assigned to response 0, which suits identical L/R drivers on a
%% single endpoint. For endpoints with distinct per-channel tuning,
%% pass num_responses > 1 and adjust assign_response accordingly.
%%
%% iir_bin / fir_bin are relative to `cpath` (the UCM staging tree),
%% iir_txt / fir_txt are full paths to the sof-ctl text dumps under
%% tools/ctl/ipc4/eq_{iir,fir}/ and are written as-is.
channels_in_config = 2;
num_responses = 1;
assign_response = [0 0];

%% IIR blob
bq_iir = sof_eq_iir_blob_quant(eq.p_z, eq.p_p, eq.p_k);
bm_iir = sof_eq_iir_blob_merge(channels_in_config, num_responses, ...
                               assign_response, bq_iir);
bp_iir = sof_eq_iir_blob_pack(bm_iir, 4);  % IPC4
sof_alsactl_write(iir_txt, bp_iir);
sof_ucm_blob_write(fullfile(cpath, iir_bin), bp_iir);

%% FIR blob
bq_fir = sof_eq_fir_blob_quant(eq.b_fir);
bm_fir = sof_eq_fir_blob_merge(channels_in_config, num_responses, ...
                               assign_response, bq_fir);
bp_fir = sof_eq_fir_blob_pack(bm_fir, 4);  % IPC4
sof_alsactl_write(fir_txt, bp_fir);
sof_ucm_blob_write(fullfile(cpath, fir_bin), bp_fir);
end

%% -----------------------------------------------------------------------
%% Small helpers used by the IIR stages
%% -----------------------------------------------------------------------
function peq = shelf_row(type, p, b)
peq = [type, clamp(p(1), b(1,1), b(1,2)), clamp(p(2), b(2,1), b(2,2)), 0];
end

function peq = pn_row(p, b, eq)
peq = [eq.PEQ_PN2, clamp(p(1), b(1,1), b(1,2)), ...
                   clamp(p(2), b(2,1), b(2,2)), ...
                   clamp(p(3), b(3,1), b(3,2))];
end

function e = stage_rms(peq_fixed, new_row, eq, fmin_fit, fmax_fit)
eq.peq = [peq_fixed; new_row];
%% Skip the FIR design pass on every fminsearch evaluation. The stage
%% objective below only looks at eq.err_db_s and eq.iir_eq_db, which
%% sof_eq_compute produces before the FIR stage, so designing the FIR
%% here would just be wasted work inside the optimization loop.
eq.enable_fir = 0;
try
    eq2 = sof_eq_compute(eq);
catch
    e = 1e6;
    return;
end
idx = eq2.f >= fmin_fit & eq2.f <= fmax_fit;
%% eq.err_db_s is the eq-smoothed (logsmooth_eq) target-minus-measurement
%% error the FIR will pick up. What remains for the FIR after the IIR is
%% err_db_s - iir_eq_db, so minimizing its shape here fits the IIR against
%% the same signal the FIR design consumes and applies logsmooth_eq (not
%% the plot-only logsmooth_plot).
residual = eq2.err_db_s(idx) - eq2.iir_eq_db(idx);
residual = residual - mean(residual);
e = sqrt(mean(residual .^ 2));
end

function y = clamp(x, lo, hi)
y = min(hi, max(lo, x));
end

%% -----------------------------------------------------------------------
%% UCM product .conf generation and misc string / filesystem helpers
%% -----------------------------------------------------------------------
function write_product_conf(conf_file, sys_vendor, product_name, endpoint, base, eq)
ep_cap = capitalize(endpoint);
iir_key = sprintf('Define.PostMixer%sPlaybackIirBlob', ep_cap);
fir_key = sprintf('Define.PostMixer%sPlaybackFirBlob', ep_cap);
iir_path = sprintf('/usr/share/alsa/ucm2/blobs/sof/ipc4/eq_iir/%s_iir.bin', base);
fir_path = sprintf('/usr/share/alsa/ucm2/blobs/sof/ipc4/eq_fir/%s_fir.bin', base);

fid = fopen(conf_file, 'w');
if fid < 0
	error('Could not open %s for writing', conf_file);
end
fprintf(fid, '# Add bespoke %s equalizer for %s %s\n', endpoint, sys_vendor, product_name);
fprintf(fid, '#\n');
fprintf(fid, '# This file was generated with %s.m\n', mfilename());
fprintf(fid, '#\n');
fprintf(fid, '# IIR is defined as parametric equalizer and FIR carries the mid-band residual\n');
fprintf(fid, '# correction, see:\n');
fprintf(fid, '# https://github.com/thesofproject/sof/tree/main/src/audio/eq_iir/tune\n');
fprintf(fid, '#\n');
fprintf(fid, '# IIR biquad stages:\n');
fprintf(fid, '#     type    fc [Hz]    gain [dB]      Q\n');
for i = 1:size(eq.peq, 1)
	fprintf(fid, '#     %-4s  %8.1f     %+6.2f     %5.2f\n', ...
	        peq_short_name(eq, eq.peq(i, 1)), ...
	        eq.peq(i, 2), eq.peq(i, 3), eq.peq(i, 4));
end
fprintf(fid, '#\n');
fprintf(fid, '# FIR parameters:\n');
fprintf(fid, '#     length      = %d taps\n', eq.fir_length);
fprintf(fid, '#     kaiser beta = %g\n', eq.fir_beta);
fprintf(fid, '#     minph       = %d\n', eq.fir_minph);
fprintf(fid, '#     band        = [%g, %g] Hz\n', eq.fmin_fir, eq.fmax_fir);
fprintf(fid, '\n');
fprintf(fid, '%s "%s"\n', iir_key, iir_path);
fprintf(fid, '%s "%s"\n', fir_key, fir_path);
fclose(fid);
fprintf('Wrote %s\n', conf_file);
end

function s = sanitize_name(field_name, s)
% Lower case, replace any non-alphanumeric run with a single underscore, and
% trim leading/trailing underscores. e.g. 'Acme Ltd.' -> 'acme_ltd',
% 'Model 100' -> 'model_100'.
%
% Error out if the result is empty (input had no ASCII letters or digits,
% e.g. a non-ASCII-only DMI value). Otherwise the blob base name would
% collapse to something like 'speaker__' and every product with such a
% DMI value would land on the same blob path.
raw = s;
s = lower(s);
s = regexprep(s, '[^a-z0-9]+', '_');
s = regexprep(s, '^_+|_+$', '');
if isempty(s)
	error(['%s "%s" has no ASCII letters or digits and would produce ' ...
	       'an empty blob name component; please pass a value that ' ...
	       'contains at least one [a-z0-9] character.'], field_name, raw);
end
end

function validate_dmi_field(field_name, s)
% Reject DMI values that would escape the output tree or otherwise produce
% a bad directory / file name when used verbatim. Path separators and the
% special names '.' and '..' are refused; any other printable UTF-8 string
% is accepted because DMI content is noisy in the wild.
if ~ischar(s) || isempty(s)
	error('%s must be a non-empty string', field_name);
end
if any(s == '/') || any(s == '\')
	error('%s must not contain path separators: "%s"', field_name, s);
end
if strcmp(s, '.') || strcmp(s, '..')
	error('%s must not be "." or "..": "%s"', field_name, s);
end
if any(s < 32)
	error('%s must not contain control characters', field_name);
end
end

function validate_endpoint(s)
% endpoint becomes part of the UCM key Define.PostMixer<Endpoint>Playback...
% and a component of blob file names, so restrict it to a plain letters-only
% identifier. Extend the pattern here if a new endpoint naming scheme is
% needed.
if ~ischar(s) || isempty(s)
	error('endpoint must be a non-empty string');
end
if isempty(regexp(s, '^[A-Za-z]+$', 'once'))
	error('endpoint must be letters only (e.g. speaker, headphone): "%s"', s);
end
end

function s = capitalize(s)
if isempty(s)
	return;
end
s = [upper(s(1)), lower(s(2:end))];
end

function ensure_dir(d)
if ~exist(d, 'dir')
	[ok, msg] = mkdir(d);
	if ~ok
		error('mkdir %s failed: %s', d, msg);
	end
end
end

function enable_common_paths(script_dir, enable)
% sof_eq_paths() addpath/rmpaths a relative '../../../../tools/tune/common'
% that is resolved against the current working directory, so it only works
% when called from src/audio/eq_iir/tune/. Do a scoped cd to script_dir so
% the (dis)enable resolves correctly regardless of the caller's CWD.
% onCleanup restores the original directory even if sof_eq_paths errors.
orig_dir = pwd();
cleanup = onCleanup(@() cd(orig_dir));  %#ok<NASGU>
cd(script_dir);
sof_eq_paths(enable);
end

function name = peq_type_name(eq, type_num)
% Look up the PEQ_* field name whose value matches type_num, using the
% constants that sof_eq_defaults() already stored on the eq struct. This
% keeps the mapping in sync with sof_eq_define_parametric_eq.m without
% duplicating the enum here.
fns = fieldnames(eq);
for k = 1:numel(fns)
	if strncmp(fns{k}, 'PEQ_', 4) && isnumeric(eq.(fns{k})) && ...
	   isscalar(eq.(fns{k})) && eq.(fns{k}) == type_num
		name = fns{k};
		return;
	end
end
name = sprintf('PEQ_%d', type_num);
end

function name = peq_short_name(eq, type_num)
% Same lookup as peq_type_name but with the 'PEQ_' prefix stripped, e.g.
% 'HP2' or 'LS2', for compact human-readable summaries in the .conf header.
name = peq_type_name(eq, type_num);
if strncmp(name, 'PEQ_', 4)
	name = name(5:end);
end
end

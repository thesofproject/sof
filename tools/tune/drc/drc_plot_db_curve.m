function drc_plot_db_curve(coefs);

if exist('OCTAVE_VERSION', 'builtin')
	pkg load control;
endif

db_thres = coefs.db_threshold;
db_knee_thres = db_thres + coefs.db_knee;

% Plot range of db_x determined by db_thres and db_knee_thres
db_x_center = (db_thres + db_knee_thres) / 2;
db_x_half_range = max(coefs.db_knee, 20);
db_x_start = db_x_center - db_x_half_range;
db_x_end = db_x_center + db_x_half_range;

db_xs = linspace(db_x_start, db_x_end, 100);
db_ys = db_curve(coefs, db_xs);

% Plot the x-y dB curve
plot(db_xs, db_ys, "b");
hold on
grid on
% Plot the reference line: x = db_thres
plot([db_thres, db_thres], [db_ys(1), db_knee_thres], "g");
hold on
% Plot the reference line: x = db_knee_thres
plot([db_knee_thres, db_knee_thres], [db_ys(1), db_knee_thres], "g");
hold on
% Plot the reference line: y = x
plot([db_xs(1), db_knee_thres], [db_xs(1), db_knee_thres], "r");
hold off
%pause();

end

function db_ys = db_curve(coefs, db_xs);

db_thres = coefs.db_threshold;
db_knee_thres = db_thres + coefs.db_knee;

db_thres_y = db_knee_curve(coefs, db_thres);
db_knee_thres_y = db_knee_curve(coefs, db_knee_thres);

for i = 1:100
	db_x = db_xs(i);
	if db_x < db_thres
		db_ys(i) = db_thres_y + (db_x - db_thres);
	elseif db_x > db_knee_thres
		db_ys(i) = db_knee_thres_y + coefs.slope * (db_x - db_knee_thres);
	else
		% Among knee curve
		db_ys(i) = db_knee_curve(coefs, db_x);
	endif
end

end

function db_y = db_knee_curve(coefs, db_x);

x = db2mag(db_x);
y = coefs.knee_alpha + coefs.knee_beta * exp(-coefs.K * x);
db_y = mag2db(y);

end

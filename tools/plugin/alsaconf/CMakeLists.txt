# SPDX-License-Identifier: BSD-3-Clause

install(FILES 50-sof.conf DESTINATION /usr/share/alsa/alsa.conf.d)
# HACK needs to link to above
install(FILES 50-sof.conf DESTINATION /etc/alsa/conf.d)

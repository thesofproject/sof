# TLV
#
# Common TLV control ranges
#

SectionTLV."vtlv_m64s2" {
	Comment "-64dB step 2dB"

	scale {
		min "-6400"
		step "200"
		mute "1"
	}
}

SectionTLV."vtlv_m50s1" {
	Comment "-50dB step 1dB"

	scale {
		min "-5000"
		step "100"
		mute "1"
	}
}

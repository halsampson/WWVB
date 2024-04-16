Experimental testing of WWVB decoding using PC audio card at ~192kHz sampling rate
(For now, SampleHz best measured precisely from 1PPS recording; TODO: auto-adjust vs. phase drift)

Antenna is shielded magnetic field loop as large as possible (now ~2 sq meters) orthogonal to Ft. Collins
with parallel capacitor chosen for ~60kHz resonance. Q as large as possible (but now < 10)

Signal then fed through a 1:20 turn transformer from dead small 5V switching power supply to boost voltage to sound card mic input, with mic boost set to +30 dB

Works best around 4:00am on West coast (see SNR dB readings in 3-5am.txt log)

Also works OK with a few bit errors during most of the day, but evening interference can be severe, likely due to strong power line ~60Hz harmonics which drift around, sometimes right through 60kHz. Best to avoid any nearby power lines.

A ferrite stick antenna from an old AM radio should also work.

References:

[https://en.wikipedia.org/wiki/WWVB#Modulation_format](https://en.wikipedia.org/wiki/WWVB#Modulation_format)

[https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904](https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904)

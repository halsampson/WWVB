Experimental testing of WWVB decoding using PC audio card at ~192kHz sampling rate
(For now, SampleHz best measured precisely from a 1PPS recording; TODO: auto-adjust vs. phase drift)

Antenna is shielded magnetic field single loop as large as possible (now ~2 sq meters) orthogonal to Ft. Collins with a parallel capacitor chosen for ~60kHz resonance. Q as large as possible for gain

Signal is then fed through a 1:10 turn transformer from a dead 5V, 2A switching power supply to boost the voltage to a sound card mic input, with mic gain boost set to +30 dB

Works best around 4:00am on West coast (see lower SNR dB readings in log*.txt)

Also works OK with a few bit errors during most of the day, but evening interference can be severe, likely due to strong power line ~60Hz harmonics which drift around, sometimes right through 60kHz. Best to avoid any nearby power lines.

A ferrite stick antenna from an old AM radio should also work.

References:

[https://en.wikipedia.org/wiki/WWVB#Modulation_format](https://en.wikipedia.org/wiki/WWVB#Modulation_format)

[https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904](https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904)

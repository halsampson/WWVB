Experimental testing of WWVB decoding using PC audio card at ~192kHz sampling rate
(For now, SampleHz best measured precisely from 1PPS recording; TODO: auto-adjust vs. phase drift)

Antenna is shielded magnetic field loop as large as possible (now ~2 sq meters) orthogonal to Ft. Collins
with capacitor chosen for ~60kHz resonance. Q as large as possible (now < 10)

Signal fed through 1:20 transformer from dead small switching power supply to boost voltage to sound card mic input mic with boost set to +30 dB

Works best around 4:00am on West coast (see SNR dB readings in 3-5am.txt)

Also works OK with a few bit errors during the day, but evening interference is high, likely due to strong power line 60Hz harmonics which drift around, sometimes right through 60 kHz. Would be better to avoid power lines now unfortunately directly in line with antenna loop.

A ferrite stick antenna from an old AM radio should also work.

References:

[https://en.wikipedia.org/wiki/WWVB#Modulation_format](https://en.wikipedia.org/wiki/WWVB#Modulation_format)

[https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904](https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=914904)

In addition to following the SHOULDs in the required sections of RFC 3208, OpenPGM also follows the MAYs. Exceptions that affect interoperability are noted for the following sections of RFC 3208:

  * Section 2.1 (transmit window advance – third MAY): OpenPGM does not delay the window advancement based on NAK silence.

  * Section 2.2 (repair requests - MAY): OpenPGM does not multicast NAKs.

  * Section 5.1.1 (MAY): OpenPGM does not use any definition of TXW\_MAX\_RTE other than that defined in the RFC.

  * Section 5.2 (SHOULD): OpenPGM does not do any negative acknowledgment (NAK) storm detection to protect against a denial of service attack. If a NAK request arrives for a sequence in the current window, a negative acknowledgment confirmation (NCF) packet is immediately sent and followed later by repair data (RDATA).

  * Section 5.3 (SHOULD): OpenPGM does not track propagation delays in computing the delay time of RDATA servicing.

  * Section 6.1 (MAY in the last paragraph): OpenPGM delivers data to the receiver application only in the order sent, not in the order received.

  * Section 6.3 (detecting missing packets – MAY): OpenPGM does not detect missing packets by multicast NAKs.

  * Section 6.3 (transmitting a NAK – SHOULD): Loss is assumed after the reception of 2 packets with sequence numbers higher than those of the assumed lost packets.

  * Section 9.4 (OPT\_JOIN): OpenPGM does not send a late joining option.

  * Section 9.5 (OPT\_REDIRECT): OpenPGM ignores redirecting POLRs.

  * Section 9.6 (both MAYs): OpenPGM does not use the synchronization (SYN) notification—either to notify the application about the start of the stream or to notify a late joining receiver whether it missed any data or did not start from the beginning of the stream.

  * Section 9.6.1 (MAY): OpenPGM does not provide any abstractions of the stream based on SYNs.

  * Section 9.7.1 (MAY in the last paragraph): OpenPGM does not provide any direct notification of the receipt of a finish (FIN) option.

  * Section 9.8.1 (MAY): OpenPGM does not provide any direct notification of the receipt of a reset (RST) option.

  * Section 11.1 (SHOULD): OpenPGM provides repair packets matching the request type, FEC on demand is not sent in response to a selective NAK.

  * Section 11.1 (MAY): OpenPGM does not generate pro-active parity packets.

  * Section 11.2 (MAY): OpenPGM does not support variable transmission groups.

  * Section 11.5 (SHOULD): OpenPGM does not bias the value of NAK\_BO\_IVL under any circumstances.

  * Section 11.5 (MAY): Prolonged incomplete transmission groups does not induce selective NAK recovery.

  * Section 12.1 (MUST): OpenPGM implements no form of congestion avoidance or fairness towards co-existing TCP flows.

  * Section 14.3 (SHOULD): OpenPGM does not conduct POLLs of any form or respond to POLL requests.

  * Section 15.2.1 (MAY): OpenPGM does not add or read NAK\_BO\_IVL tuning parameters from NCFs or SPMs.

  * Section 16.1 (third MAY): OpenPGM does not reset TXW\_ADV\_IVL\_TMR if NAKs are received.

  * Section 16.4 (MAY): OpenPGM does not provide any other method of advancing the transmit window other than advancing with data as described under section 16.2.

package com.phonecastvr.sender;
import org.junit.Test;
import static org.junit.Assert.*;
public class DiscoveryProtocolTest {
    private final String nonce = "0123456789abcdef0123456789abcdef";
    private String reply(String port, String name) { return "PCVR_RECEIVER 1 " + nonce + " " + port + " 0.9.0 " + name; }
    @Test public void receiverHintsAreBoundedAndCorrelated() {
        assertEquals("PCVR_DISCOVER 1 " + nonce, DiscoveryProtocol.query(nonce));
        assertArrayEquals(new String[]{"0.9.0", "Living room Frame"}, DiscoveryProtocol.parse(reply("49321", "Living room Frame"), nonce));
        assertNull(DiscoveryProtocol.parse(reply("49321", "Frame"), "wrong"));
        assertNull(DiscoveryProtocol.parse(reply("0", "Frame"), nonce));
        assertNull(DiscoveryProtocol.parse(reply("49323", "Frame"), nonce));
        assertNull(DiscoveryProtocol.parse(reply("49321", "Frame\nforged"), nonce));
        assertNull(DiscoveryProtocol.parse(reply("49321", new String(new char[49]).replace('\0','x')), nonce));
        assertNull(DiscoveryProtocol.parse("PCVR_RECEIVER 2 " + nonce + " 49321 0.9.0 Frame", nonce));
    }
}

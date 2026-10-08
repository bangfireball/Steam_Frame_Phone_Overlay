package com.phonecastvr.sender;

final class DiscoveryProtocol {
    static final int PORT = 49322;
    static String query(String nonce) { return "PCVR_DISCOVER 1 " + nonce; }
    static String[] parse(String text, String nonce) {
        if (text.length() > 160) return null;
        String[] fields = text.split(" ", 6);
        if (fields.length != 6 || !fields[0].equals("PCVR_RECEIVER") ||
                !fields[1].equals("1") || !fields[2].equals(nonce)) return null;
        try {
            int port = Integer.parseInt(fields[3]);
            if (port != StreamProtocol.DEFAULT_PORT) return null; // Current Android TCP port is fixed.
        } catch (NumberFormatException error) { return null; }
        if (!fields[4].matches("[0-9.]{1,16}") || !fields[5].matches("[A-Za-z0-9_. -]{1,48}")) return null;
        return new String[]{fields[4], fields[5]};
    }
    private DiscoveryProtocol() {}
}

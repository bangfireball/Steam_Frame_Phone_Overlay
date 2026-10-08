package com.phonecastvr.sender;

import java.net.ConnectException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;

/** Text is fixed and safe to export: never contains addresses or credentials. */
final class ConnectionFeedback {
    static final String CONNECTING = "Connecting to receiver…";
    static final String WAITING = "Receiver not confirmed · check headset pairing code";
    static final String CONNECTED = "Connected · receiver responding";
    static final String CLOSED = "Receiver disconnected · check pairing code and relaunch PhoneCast on the headset";
    static String failure(Exception error) {
        if (error instanceof UnknownHostException) return "Address not found · check receiver IP or Find headset";
        if (error instanceof SocketTimeoutException) return "Receiver not responding · check same non-guest LAN, VPN and headset IP";
        if (error instanceof ConnectException) return "Cannot reach receiver · launch PhoneCast on headset and check IP/network";
        if ("Unsupported PhoneCast protocol version".equals(error.getMessage()))
            return "Receiver protocol incompatible · update both apps from the same release";
        return CLOSED;
    }
    private ConnectionFeedback() {}
}

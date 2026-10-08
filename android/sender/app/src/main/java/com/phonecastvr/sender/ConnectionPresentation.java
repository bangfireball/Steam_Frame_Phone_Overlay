package com.phonecastvr.sender;

/** Short stable heading; actionable details are shown in a separately sized field. */
final class ConnectionPresentation {
    static String title(boolean responsive, String message) {
        if (responsive) return "Connected to receiver";
        if (ConnectionFeedback.CONNECTING.equals(message)) return "Connecting to receiver";
        if (ConnectionFeedback.WAITING.equals(message)) return "Waiting for receiver reply";
        return "Not connected · retrying";
    }
    private ConnectionPresentation() {}
}

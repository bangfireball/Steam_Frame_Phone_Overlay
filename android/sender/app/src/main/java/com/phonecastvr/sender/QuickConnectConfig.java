package com.phonecastvr.sender;

final class QuickConnectConfig {
    private QuickConnectConfig() {}

    static boolean isReady(String receiverHost, String pairCode) {
        return receiverHost != null && !receiverHost.trim().isEmpty() &&
                StreamProtocol.validPairCode(pairCode);
    }
}

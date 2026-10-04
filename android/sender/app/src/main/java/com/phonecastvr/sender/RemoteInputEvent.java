package com.phonecastvr.sender;

final class RemoteInputEvent {
    static final int DOWN = 1;
    static final int MOVE = 2;
    static final int UP = 3;
    static final int SCROLL = 4;
    static final int BACK = 5;

    final int type;
    final float normalizedX;
    final float normalizedY;
    final float scrollDelta;
    final long sequence;

    RemoteInputEvent(int type, float normalizedX, float normalizedY,
                     float scrollDelta, long sequence) {
        this.type = type;
        this.normalizedX = normalizedX;
        this.normalizedY = normalizedY;
        this.scrollDelta = scrollDelta;
        this.sequence = sequence;
    }
}

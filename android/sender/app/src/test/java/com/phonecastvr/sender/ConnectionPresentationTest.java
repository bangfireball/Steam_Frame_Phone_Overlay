package com.phonecastvr.sender;
import org.junit.Test;
import static org.junit.Assert.*;
public class ConnectionPresentationTest {
    @Test public void failureReasonsShareAStableShortHeading() {
        assertEquals("Not connected · retrying", ConnectionPresentation.title(false, ConnectionFeedback.CLOSED));
        assertEquals("Not connected · retrying", ConnectionPresentation.title(false,
                ConnectionFeedback.failure(new java.net.ConnectException())));
        assertEquals("Connecting to receiver", ConnectionPresentation.title(false, ConnectionFeedback.CONNECTING));
        assertEquals("Waiting for receiver reply", ConnectionPresentation.title(false, ConnectionFeedback.WAITING));
        assertEquals("Connected to receiver", ConnectionPresentation.title(true, ConnectionFeedback.CLOSED));
    }
}

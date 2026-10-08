package com.phonecastvr.sender;
import org.junit.Test;
import static org.junit.Assert.*;
import java.net.*;
public class ConnectionFeedbackTest {
    @Test public void errorsAreActionableAndDoNotExportExceptionText() {
        assertTrue(ConnectionFeedback.failure(new ConnectException("secret 123456")).contains("launch PhoneCast"));
        assertTrue(ConnectionFeedback.failure(new SocketTimeoutException("private address")).contains("VPN"));
        assertTrue(ConnectionFeedback.failure(new UnknownHostException("private address")).contains("Find headset"));
        assertFalse(ConnectionFeedback.failure(new java.io.IOException("123456 private data")).contains("123456"));
        assertFalse(ConnectionFeedback.WAITING.equals(ConnectionFeedback.CONNECTED));
    }
}

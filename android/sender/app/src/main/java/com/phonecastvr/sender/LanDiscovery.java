package com.phonecastvr.sender;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Looper;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;

/** Explicit, bounded search. Replies are untrusted hints, never authentication. */
final class LanDiscovery {
    interface Listener { void finished(ArrayList<String[]> receivers, boolean failed); }
    private volatile DatagramSocket socket;
    private volatile boolean cancelled;
    void cancel() { cancelled = true; DatagramSocket current = socket; if (current != null) current.close(); }
    void start(Context context, Listener listener) {
        new Thread(() -> {
            ArrayList<String[]> found = new ArrayList<>();
            Set<String> seen = new LinkedHashSet<>();
            WifiManager.MulticastLock lock = null;
            boolean failed = false;
            try {
                WifiManager wifi = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                if (wifi != null) { lock = wifi.createMulticastLock("phonecast-discovery"); lock.setReferenceCounted(false); lock.acquire(); }
                Set<InetAddress> destinations = new LinkedHashSet<>();
                destinations.add(InetAddress.getByName("255.255.255.255"));
                java.util.Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
                while (interfaces != null && interfaces.hasMoreElements()) {
                    NetworkInterface item = interfaces.nextElement();
                    if (!item.isUp() || item.isLoopback()) continue;
                    for (InterfaceAddress address : item.getInterfaceAddresses())
                        if (address.getBroadcast() != null && destinations.size() < 16) destinations.add(address.getBroadcast());
                }
                try (DatagramSocket udp = new DatagramSocket()) {
                    socket = udp;
                    if (cancelled) return;
                    udp.setBroadcast(true); udp.setSoTimeout(250);
                    String nonce = UUID.randomUUID().toString().replace("-", "");
                    byte[] query = DiscoveryProtocol.query(nonce).getBytes(StandardCharsets.US_ASCII);
                    long end = android.os.SystemClock.elapsedRealtime() + 4000;
                    long nextSend = 0;
                    int packets = 0;
                    while (!cancelled && android.os.SystemClock.elapsedRealtime() < end && packets < 128) {
                        long now = android.os.SystemClock.elapsedRealtime();
                        if (now >= nextSend) {
                            for (InetAddress destination : destinations)
                                udp.send(new DatagramPacket(query, query.length, destination, DiscoveryProtocol.PORT));
                            nextSend = now + 1000;
                        }
                        DatagramPacket reply = new DatagramPacket(new byte[161], 161);
                        try { udp.receive(reply); } catch (SocketTimeoutException timeout) { continue; }
                        ++packets;
                        if (!(reply.getAddress() instanceof Inet4Address) || reply.getAddress().isLoopbackAddress()) continue;
                        String[] parsed = DiscoveryProtocol.parse(new String(reply.getData(), 0, reply.getLength(), StandardCharsets.US_ASCII), nonce);
                        String ip = reply.getAddress().getHostAddress();
                        if (parsed != null && seen.add(ip) && found.size() < 32) found.add(new String[]{ip, parsed[1], parsed[0]});
                    }
                }
            } catch (Exception error) { failed = true; }
            finally { socket = null; if (lock != null && lock.isHeld()) lock.release(); }
            final boolean resultFailed = failed;
            if (!cancelled) new Handler(Looper.getMainLooper()).post(() -> { if (!cancelled) listener.finished(found, resultFailed); });
        }, "phonecast-discovery").start();
    }
}

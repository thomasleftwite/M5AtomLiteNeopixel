package jp.atomneopixel.ble;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

final class LoopbackServer implements Closeable {
    static final String HOST = "127.0.0.1";
    static final int PORT = 8080;
    static final String URL = "http://" + HOST + ":" + PORT + "/";
    private static final String HOST_HEADER = HOST + ":" + PORT;
    private static final byte[] HEADER_END = {'\r', '\n', '\r', '\n'};
    private static final byte[] EMPTY = new byte[0];
    private final ServerSocket listener;
    private final byte[] page;
    private final Set<Socket> openClients = new HashSet<>();
    private final ThreadPoolExecutor clients = new ThreadPoolExecutor(
            4, 4, 0, TimeUnit.SECONDS, new ArrayBlockingQueue<>(8));
    private boolean closing;

    LoopbackServer(byte[] page) throws IOException {
        this.page = page;
        ServerSocket socket = new ServerSocket();
        try {
            socket.setReuseAddress(true);
            socket.bind(new InetSocketAddress(InetAddress.getByName(HOST), PORT));
        } catch (IOException error) {
            socket.close();
            throw error;
        }
        listener = socket;
    }

    void serve() throws IOException {
        while (!listener.isClosed()) {
            Socket client = listener.accept();
            queueResponse(client);
        }
    }

    private synchronized void queueResponse(Socket client) throws IOException {
        if (closing) {
            client.close();
            return;
        }
        openClients.add(client);
        try {
            clients.execute(() -> respond(client));
        } catch (RejectedExecutionException error) {
            openClients.remove(client);
            client.close();
        }
    }

    private void respond(Socket client) {
        try (Socket socket = client) {
            socket.setSoTimeout(2000);
            String header = readHeader(new BufferedInputStream(socket.getInputStream()));
            if (header == null) return;
            String[] lines = header.split("\\r\\n", -1);
            boolean getRoot = lines[0].equals("GET / HTTP/1.1") || lines[0].equals("GET / HTTP/1.0");
            boolean hostMatches = false;
            for (int i = 1; i < lines.length; i++) {
                if (lines[i].regionMatches(true, 0, "Host:", 0, 5)) {
                    hostMatches = lines[i].substring(5).trim().equals(HOST_HEADER);
                    break;
                }
            }
            byte[] body = getRoot && hostMatches ? page : EMPTY;
            String status = getRoot && hostMatches ? "200 OK" : "404 Not Found";
            OutputStream output = socket.getOutputStream();
            output.write(("HTTP/1.1 " + status + "\r\nContent-Type: text/html; charset=utf-8\r\n"
                    + "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
                    + "Content-Length: " + body.length + "\r\nConnection: close\r\n\r\n")
                    .getBytes(StandardCharsets.US_ASCII));
            output.write(body);
        } catch (IOException ignored) {
            // A browser may close a speculative connection before sending a request.
        } finally {
            synchronized (this) {
                openClients.remove(client);
            }
        }
    }

    private static String readHeader(BufferedInputStream input) throws IOException {
        ByteArrayOutputStream header = new ByteArrayOutputStream();
        int matched = 0;
        for (int remaining = 8192; remaining > 0; remaining--) {
            int next = input.read();
            if (next < 0) return null;
            header.write(next);
            if (next == HEADER_END[matched]) {
                if (++matched == HEADER_END.length) {
                    return new String(header.toByteArray(), StandardCharsets.US_ASCII);
                }
            } else {
                matched = next == '\r' ? 1 : 0;
            }
        }
        return null;
    }

    @Override
    public synchronized void close() throws IOException {
        if (closing) return;
        closing = true;
        clients.shutdownNow();
        for (Socket client : openClients) {
            try {
                client.close();
            } catch (IOException ignored) {
                // Continue closing the remaining connections.
            }
        }
        openClients.clear();
        listener.close();
    }
}

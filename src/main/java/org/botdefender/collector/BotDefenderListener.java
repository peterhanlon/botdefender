package org.botdefender.collector;

import org.apache.log4j.Logger;

import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;


public class BotDefenderListener
{
    private static Logger logger = Logger.getLogger(BotDefenderListener.class);
    private BlockingQueue<String> queue;
    private static int LISTENER_PORT=9090;
    private static int DEFAULT_MAX_QUEUE_SIZE=1000;


    private BotDefenderListener() {
        queue = new ArrayBlockingQueue<String>(DEFAULT_MAX_QUEUE_SIZE);
    }


    public void launchIncomingMessageThread() throws Exception {
        logger.info("Starting socket listener on port "+LISTENER_PORT);
        ServerSocket serverSocket = new ServerSocket(LISTENER_PORT);

        // Start the socket listener waiting for incoming requests from mod_collector
        while (true) {
            Socket socket = serverSocket.accept();
            IncomingMessageThread thread = new IncomingMessageThread(socket, queue);
            thread.start();
        }
    }

    private void launchMessageWriterThread() throws Exception {
        MessageWriterThread thread = new MessageWriterThread(queue);
        thread.start();
    }


    private void run() throws Exception {
        launchMessageWriterThread();
        launchIncomingMessageThread();
    }


    public static void main(String args[]) throws Exception {
        BotDefenderListener app = new BotDefenderListener();
        app.run();
    }
}

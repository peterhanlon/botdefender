package org.botdefender.collector;

import org.apache.log4j.Logger;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.Socket;
import java.util.concurrent.BlockingQueue;


/**
 * Created with IntelliJ IDEA.
 * User: pete.hanlon
 * Date: 18/08/14
 * Time: 19:13
 * To change this template use File | Settings | File Templates.
 */
public class IncomingMessageThread extends Thread {

    private static Logger logger = Logger.getLogger(IncomingMessageThread.class);
    private BlockingQueue<String> queue;
    private Socket socket;

    IncomingMessageThread(Socket socket, BlockingQueue<String> queue) throws Exception {
        this.socket = socket;
        this.queue = queue;
    }



    public void run() {
        BufferedReader br = null;
        try {
            br = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            logger.info("Opening data channel on port " + socket.getPort());
            String jsonData;

            // Get the JSON message from apache mod_collector
            while ((jsonData = br.readLine()) != null) {
                if (jsonData.equals("")) {
                     break;
                }

                try {
                    queue.add(jsonData);
                } catch (Exception e) {
                    logger.error(e);
                }
            }
        } catch (Exception e) {
            logger.error(e);
        } finally {
            logger.info("Closing data channel on port " + socket.getPort());
            try {
                br.close();
            } catch (Exception e) {
                logger.error("Error closing inFromClient BufferedReader: " + e);
            }
            try {
                socket.close();
            } catch (IOException e) {
                logger.error("Error closing receiver socket: " + e);
            }
        }
    }
}


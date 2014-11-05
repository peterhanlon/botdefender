package org.botdefender.collector;

import org.apache.log4j.Logger;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.Socket;
import java.util.concurrent.BlockingQueue;


/*
 * The MIT License (MIT)
 *
 * Copyright (c)2014 Peter Hanlon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
public class IncomingMessageThread extends Thread {

    private static Logger logger = Logger.getLogger(IncomingMessageThread.class);
    private CollectorConfiguration config;
    private BlockingQueue<String> queue;
    private Socket socket;
    private static int msgCnt=0;

    IncomingMessageThread(CollectorConfiguration config, Socket socket, BlockingQueue<String> queue) throws Exception {
        this.config = config;
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
                    if (queue.remainingCapacity()>0) {
                        logger.info((msgCnt++)+" "+jsonData);
                        queue.add(jsonData);
                    } else {
                        logger.warn("Queue has reached capacity, clearing queue");
                        queue.clear();
                    }
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


package org.botdefender.collector;

import org.apache.log4j.Logger;

import java.io.InputStreamReader;
import java.io.Reader;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

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
public class BotDefenderListener
{
    private static Logger logger = Logger.getLogger(BotDefenderListener.class);
    private CollectorConfiguration config;
    private BlockingQueue<String> messageQueue;


    private BotDefenderListener() throws Exception {
        this.config = loadConfiguration();
        this.messageQueue = new ArrayBlockingQueue<String>(config.getMessageQueueSize());
    }


    private CollectorConfiguration loadConfiguration() throws Exception {
        Gson gson = new GsonBuilder().create();
        Reader reader = new InputStreamReader(BotDefenderListener.class.getResourceAsStream("/collector.json"), "UTF-8");
        CollectorConfiguration config = gson.fromJson(reader, CollectorConfiguration.class);
        return config;
    }


    public void launchIncomingMessageThread() throws Exception {
        logger.info("Starting socket listener on port "+config.getListenerPort());
        ServerSocket serverSocket = new ServerSocket(config.getListenerPort());

        // Start the socket listener waiting for incoming requests from mod_collector
        while (true) {
            Socket socket = serverSocket.accept();
            IncomingMessageThread thread = new IncomingMessageThread(config, socket, messageQueue);
            thread.start();
        }
    }

    private void launchMessageWriterThread() throws Exception {
        MessageWriter persistence = new MongoMessageWriter(config);
        MessageWriterThread thread = new MessageWriterThread(config, messageQueue, persistence);
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

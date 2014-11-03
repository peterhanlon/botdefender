package org.botdefender.collector;

import org.apache.log4j.Logger;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

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
public class MessageWriterThread extends Thread {
    private static Logger logger = Logger.getLogger(MessageWriterThread.class);
    private CollectorConfiguration config;
    private static int MAX_ELAPSE_TIME_MS = 1000;
    private BlockingQueue<String> queue;
    private MessageWriter messageWriter;



    public MessageWriterThread(CollectorConfiguration config, BlockingQueue<String> queue, MessageWriter messageWriter) throws Exception {
        this.config = config;
        this.messageWriter = messageWriter;
        this.queue = queue;
    }


    public void run() {
        try {
            while (true) {
                String jsonData = queue.poll(MAX_ELAPSE_TIME_MS, TimeUnit.MILLISECONDS);
                messageWriter.writeHit(jsonData);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

package org.botdefender.collector;

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
public class CollectorConfiguration {
    private int listenerPort;
    private int messageQueueSize;
    private int messageWriterMaxBatchSize;
    private String writerCollectionName;
    private String writerDatabaseName;
    private long writerMaxCollectionSizeBytes;

    public int getListenerPort() {
        return listenerPort;
    }

    public void setListenerPort(int listenerPort) {
        this.listenerPort = listenerPort;
    }

    public int getMessageQueueSize() {
        return messageQueueSize;
    }

    public void setMessageQueueSize(int messageQueueSize) {
        this.messageQueueSize = messageQueueSize;
    }

    public int getMessageWriterMaxBatchSize() {
        return messageWriterMaxBatchSize;
    }

    public void setMessageWriterMaxBatchSize(int messageWriterMaxBatchSize) {
        this.messageWriterMaxBatchSize = messageWriterMaxBatchSize;
    }

    public String getWriterCollectionName() {
        return writerCollectionName;
    }

    public void setWriterCollectionName(String writerCollectionName) {
        this.writerCollectionName = writerCollectionName;
    }

    public String getWriterDatabaseName() {
        return writerDatabaseName;
    }

    public void setWriterDatabaseName(String writerDatabaseName) {
        this.writerDatabaseName = writerDatabaseName;
    }

    public long getWriterMaxCollectionSizeBytes() {
        return writerMaxCollectionSizeBytes;
    }

    public void setWriterMaxCollectionSizeBytes(long writerMaxCollectionSizeBytes) {
        this.writerMaxCollectionSizeBytes = writerMaxCollectionSizeBytes;
    }
}

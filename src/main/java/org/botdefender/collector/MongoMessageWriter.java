package org.botdefender.collector;

import com.mongodb.*;
import com.mongodb.util.JSON;
import org.apache.log4j.Logger;

import java.util.ArrayList;
import java.util.List;

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
public class MongoMessageWriter implements MessageWriter {
    private static Logger logger = Logger.getLogger(MongoMessageWriter.class);
    private CollectorConfiguration config;
    private DBCollection collection = null;
    private List<DBObject> jsonList;


    public MongoMessageWriter(CollectorConfiguration config) {
        this.config = config;
        jsonList = new ArrayList<DBObject>();
    }



    private DBCollection connectHitsCollection() throws Exception {
        DB db = new MongoClient().getDB(config.getWriterDatabaseName());
        if (db.collectionExists(config.getWriterCollectionName())) {
            // Connect to the collection
            collection = db.getCollection(config.getWriterCollectionName());
        } else {
            // Create a capped collection so the database doesn't grow forever
            DBObject options = BasicDBObjectBuilder.start().add("capped", true).add("size", config.getWriterMaxCollectionSizeBytes()).get();
            collection = db.createCollection(config.getWriterCollectionName(), options);
        }
        return collection;
    }



    public void writeHit(String json) throws Exception {
        try {
            if (collection == null) {
                logger.info("Connecting to the mongodb database");
                collection = connectHitsCollection();
                logger.info("successfully connected");
            }

            if (json != null) {
                jsonList.add((DBObject) JSON.parse(json));
            }

            if ((json == null && jsonList.size()>0) || (jsonList.size() > config.getMessageWriterMaxBatchSize())) {
                collection.insert(jsonList,WriteConcern.ACKNOWLEDGED);
                jsonList.clear();
                logger.info("INSERTING INTO DATABASE");
            }
        } catch (Exception e) {
            // We can throw away old events if the queue is full
            logger.error("Error trying to write data, trying again [" + e.getMessage() + "]");
            collection =null;
            Thread.sleep(1000);
        }
    }
}

package org.botdefender.collector;

import com.mongodb.DB;
import com.mongodb.DBCollection;
import com.mongodb.DBObject;
import com.mongodb.MongoClient;
import com.mongodb.util.JSON;
import org.apache.log4j.Logger;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Created with IntelliJ IDEA.
 * User: pete.hanlon
 * Date: 19/08/14
 * Time: 07:11
 * To change this template use File | Settings | File Templates.
 */
public class MessageWriterThread extends Thread {
    private static Logger logger = Logger.getLogger(MessageWriterThread.class);
    private static int MAX_ELAPSE_TIME = 1000;
    private static int MAX_BATCH_SIZE=100;
    private BlockingQueue<String> queue;
    private DBCollection hitsCollection;
    private List<DBObject> objectBatch;



    public MessageWriterThread(BlockingQueue<String> queue) throws Exception {
        DB db = new MongoClient().getDB("botdefender");
        this.hitsCollection = db.getCollection("hits");
        this.objectBatch = new ArrayList<DBObject>();
        this.queue = queue;
    }


    public void run() {
        try {
            long batchSize = 0;

            while (true) {
                String jsonData = queue.poll(MAX_ELAPSE_TIME, TimeUnit.SECONDS);

                // Add the jsonData to the insertBatch list
                if (jsonData != null) {
                    objectBatch.add((DBObject) JSON.parse(jsonData));
                }

                // Commit the insertBatch batch to MongoDB
                if ((jsonData == null && objectBatch.size()>0)
                        || (batchSize>MAX_BATCH_SIZE)) {
                    hitsCollection.insert(objectBatch);
                    objectBatch.clear();
                    batchSize=0;
                } else {
                    batchSize++;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

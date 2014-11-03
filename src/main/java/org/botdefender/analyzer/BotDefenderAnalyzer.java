package org.botdefender.analyzer;

import com.mongodb.DB;
import com.mongodb.MongoClient;
import org.apache.log4j.Logger;
import org.jongo.Jongo;
import org.jongo.MongoCollection;

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
public class BotDefenderAnalyzer {
    private static Logger logger = Logger.getLogger(BotDefenderAnalyzer.class);

    private void createIndexes() throws Exception {
        DB db = new MongoClient().getDB("botdefender");
        Jongo jongo = new Jongo(db);
        jongo.getCollection("botdefender").ensureIndex("{'hit.sessionMD5':1}");
        jongo.getCollection("botdefender").ensureIndex("{'request.ip.client':1}");
        jongo.getCollection("botdefender").ensureIndex("{'hit.sessionState':1}");
        jongo.getCollection("botdefender").ensureIndex("{'response.statusCode':1}");
        jongo.getCollection("botdefender").ensureIndex("{'request.headers.Referer':1}");
        System.out.println("Indexes created");
    }

    private void findHitsGroupedByField(String description, String field , int limit) throws Exception {
        DB db = new MongoClient().getDB("botdefender");
        Jongo jongo = new Jongo(db);
        MongoCollection hits = jongo.getCollection("hits");

        List<SessionMatch> sessions  = hits.aggregate("{$group : {_id: '"+field+"', counter: {$sum: 1}}}")
                    .and("{$sort : {counter: -1}}")
                    .and("{$limit : #}", limit)
                    .as(SessionMatch.class);

        System.out.println("*** " + description);
        for(SessionMatch session: sessions) {
            System.out.println(description + "\t" + session.getId() + "\t" + session.getCounter());
        }
    }



    private void findHitsGroupedByFieldWithMatch(String description, String field, String match, int limit) throws Exception {
        DB db = new MongoClient().getDB("botdefender");
        Jongo jongo = new Jongo(db);
        MongoCollection hits = jongo.getCollection("hits");
        List<SessionMatch> sessions  = hits.aggregate("{$match : "+match+"}")
                .and("{$group : {_id: '"+field+"', counter: {$sum: 1}}}")
                .and("{$sort : {counter: -1}}")
                .and("{$limit : #}", limit)
                .as(SessionMatch.class);

        System.out.println("*** " + description);
        for(SessionMatch session: sessions) {
            System.out.println(description + "\t" + session.getId() + "\t" + session.getCounter());
        }
    }


    public static void main(String args[]) {
        try {
            BotDefenderAnalyzer app = new BotDefenderAnalyzer();

            app.createIndexes();

            // Find sessions with a lot of hits
            app.findHitsGroupedByField("Session with lots of hits", "$hit.sessionMD5", 10);

            // Check for a high number of requests with a missing referer
            app.findHitsGroupedByFieldWithMatch("Session with missing referer", "$hit.sessionMD5", "{request.headers.Referer : { $exists : false }}", 10);

            // Check for a high number of requests with non 200 or 300 HTTP response codes
            app.findHitsGroupedByFieldWithMatch("Session with error HTTP codes", "$hit.sessionMD5", "{response.statusCode : { $gt : 400, $lt : 600}}", 10);

            // Find an IP with a lot of hits
            app.findHitsGroupedByField("IP with lots of hits", "$request.ip.client", 10);

            // Check for a high number of requests with a missing referer
            app.findHitsGroupedByFieldWithMatch("IP with missing referer", "$request.ip.client", "{request.headers.Referer : { $exists : false }}", 10);

            // Check for a high number of requests with non 200 or 300 HTTP response codes
            app.findHitsGroupedByFieldWithMatch("IP with error HTTP codes", "$request.ip.client", "{response.statusCode : { $gt : 400, $lt : 600}}", 10);

            // Check for a high number of requests with a MISSING sessionMD5
            app.findHitsGroupedByFieldWithMatch("IP with MISSING session MD5", "$request.ip.client", "{hit.sessionState : 'MISSING'}", 10);


        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

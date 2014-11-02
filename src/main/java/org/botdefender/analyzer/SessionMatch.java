package org.botdefender.analyzer;

import org.jongo.marshall.jackson.oid.Id;
import org.jongo.marshall.jackson.oid.ObjectId;

/**
 * Created with IntelliJ IDEA.
 * User: pete
 * Date: 31/10/14
 * Time: 20:27
 * To change this template use File | Settings | File Templates.
 */
public class SessionMatch {
    @ObjectId
    @Id
    private String id;

    private String url;

    private long counter;

    public String getUrl() {
        return url;
    }

    public void setUrl(String url) {
        this.url = url;
    }

    public long getCounter() {
        return counter;
    }

    public void setCounter(long counter) {
        this.counter = counter;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }
}
